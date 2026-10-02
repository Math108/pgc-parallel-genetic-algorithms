#!/bin/bash
cd "$(dirname "$0")"
ROOT_DIR="$(pwd)"

echo "=========================================="
echo " 🧪 ORQUESTRADOR DINÂMICO DE TESTES "
echo "=========================================="

# Limpa o arquivo de rastreamento antigo
rm -f "$ROOT_DIR/recent_reports.txt"

# Busca todos os arquivos .c e .cu
mapfile -t ARQUIVOS < <(find src -type f \( -name "*.c" -o -name "*.cu" \) | sort)

if [ ${#ARQUIVOS[@]} -eq 0 ]; then
    echo "❌ Nenhum arquivo .c ou .cu encontrado na pasta src/."
    exit 1
fi

echo "Arquivos disponíveis para teste:"
for i in "${!ARQUIVOS[@]}"; do
    echo "  $((i+1))) ${ARQUIVOS[$i]}"
done

echo "------------------------------------------"
echo "Pressione ENTER (deixando vazio) para testar TODOS."
read -p "1. Digite os números dos códigos (separados por espaço): " ESCOLHAS

# Se a variável estiver vazia, gera uma sequência com todos os números
if [ -z "$ESCOLHAS" ]; then
    echo "Nenhuma opção digitada. Selecionando TODOS os arquivos..."
    ESCOLHAS=$(seq 1 ${#ARQUIVOS[@]} | tr '\n' ' ')
fi

echo "------------------------------------------"
echo "Escolha o modo de teste:"
echo "  1) Aleatório (Gerar novas linhas)"
echo "  2) Comparativo (Linhas fixas, 10 Sudokus, 5 repetições)"
read -p "Modo (1 ou 2): " MODO_TESTE

if [ "$MODO_TESTE" == "2" ]; then
    Y_REPETICOES=5
    SEED_FILE="$ROOT_DIR/data/linhas_de_teste_fixas.txt"
    
    if [ ! -f "$SEED_FILE" ]; then
        echo "❌ Erro: O arquivo $SEED_FILE não existe!"
        exit 1
    fi
    echo "✅ Modo Comparativo ativado. Utilizando arquivo fixo com $Y_REPETICOES repetições."
else
    read -p "2. Quantos Sudokus distintos sortear? (X): " X_SUDOKUS
    read -p "3. Quantas repeticoes por Sudoku? (Y): " Y_REPETICOES

    SEED_FILE="$ROOT_DIR/data/linhas_de_teste.txt"
    shuf -i 1-3000000 -n $X_SUDOKUS > "$SEED_FILE"
    echo "✅ Arquivo de sementes gerado em: $SEED_FILE"
fi

for NUM in $ESCOLHAS; do
    IDX=$((NUM-1))
    
    if [ -z "${ARQUIVOS[$IDX]}" ]; then
        echo "⚠️ Opção $NUM inválida. Pulando..."
        continue
    fi

    FILE_PATH="${ARQUIVOS[$IDX]}"
    FILE_NAME="$(basename "$FILE_PATH")"
    DIR_NAME="$(dirname "$FILE_PATH")"
    EXEC_NAME="${FILE_NAME%.*}" 
    
    echo "=========================================="
    echo "🚀 INICIANDO BATERIA: $FILE_NAME"
    echo "📂 Local: $DIR_NAME"
    
    if [[ "$DIR_NAME" == *"Sequential"* ]]; then
        COMP="gcc -O3 $FILE_NAME -o output/$EXEC_NAME"
        REP="python3 generate_report.py"
    elif [[ "$DIR_NAME" == *"Omp"* ]]; then
        COMP="gcc -fopenmp -O3 $FILE_NAME -o output/$EXEC_NAME"
        REP="python3 generate_report.py"
    elif [[ "$DIR_NAME" == *"Cuda"* ]]; then
        COMP="nvcc -O3 $FILE_NAME -o output/$EXEC_NAME"
        REP="python3 generate_report_cuda.py"
    else
        continue
    fi

    cd "$DIR_NAME"
    mkdir -p output Graphs
    rm -rf Graphs/*
    
    echo "⚙️ Compilando..."
    $COMP

    if [ $? -ne 0 ]; then
        echo "❌ Erro na compilação de $FILE_NAME. Pulando para o próximo..."
        cd "$ROOT_DIR"
        continue
    fi

    while IFS= read -r LINHA; do
        if [ -z "$LINHA" ]; then continue; fi
        for RUN in $(seq 1 $Y_REPETICOES); do
            echo "▶️ [$EXEC_NAME] Linha: $LINHA | Repetição: $RUN/$Y_REPETICOES"
            mkdir -p "Graphs/problem_${LINHA}_run_${RUN}"
            
            ./output/$EXEC_NAME $LINHA 1 $RUN
            python3 plot_fitness.py $LINHA $RUN
        done
    done < "$SEED_FILE"
    
    echo "📊 Gerando Dashboard da arquitetura..."
    $REP
    
    LATEST_REPORT=$(ls -td "$ROOT_DIR/$DIR_NAME"/Reports/*/Dashboard_Master*.txt | head -1)
    
    if [ -n "$LATEST_REPORT" ]; then
        REL_PATH="${LATEST_REPORT#$ROOT_DIR/}"
        echo "$REL_PATH|$EXEC_NAME" >> "$ROOT_DIR/recent_reports.txt"
    else
        echo "⚠️ Aviso: Dashboard de $EXEC_NAME não foi localizado para o rastreio."
    fi
    
    cd "$ROOT_DIR"
done

echo "📈 Executando consolidacao global..."
python3 relatorio_agregado.py
echo "✅ Orquestracao finalizada!"