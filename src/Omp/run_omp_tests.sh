#!/bin/bash

cd "$(dirname "$0")"

if [ -z "$1" ]; then
    echo "⚠️ Uso correto: ./run_parallel_tests.sh <quantidade_de_testes>"
    exit 1
fi

TOTAL=$1

GRAPHS_DIR="Graphs"
if [ -d "$GRAPHS_DIR" ]; then
    echo "Limpando execucoes paralelas anteriores..."
    rm -rf "$GRAPHS_DIR"/*
else
    mkdir -p "$GRAPHS_DIR"
fi

OUTPUT_DIR="output"
mkdir -p "$OUTPUT_DIR"

echo "⚙️  Compilando Omp.c com suporte a multi-threading (-fopenmp) e -O3..."
gcc -fopenmp -O3 Omp.c -o "$OUTPUT_DIR/omp"

if [ $? -ne 0 ]; then
    echo "❌ Erro na compilação do OpenMP. Abortando."
    exit 1
fi

echo "🚀 Iniciando bateria paralela de $TOTAL execuções..."
echo "=================================================="

for i in $(seq 1 $TOTAL); do
    
    LINHA=$(shuf -i 1-3000000 -n 1)
    
    echo "▶️  [OMP] Execução $i de $TOTAL | Sorteada a linha: $LINHA"
    
    PASTA_DESTINO="$GRAPHS_DIR/problem_$LINHA"
    mkdir -p "$PASTA_DESTINO"
    
    START=$(date +%s.%N)
    
    ./$OUTPUT_DIR/omp $LINHA 1 
    
    python3 plot_fitness.py $LINHA
    
    END=$(date +%s.%N)
    TEMPO=$(awk "BEGIN {print $END - $START}")
    printf "⏱️  Tempo absoluto de parede: %.3f segundos\n" "$TEMPO"
    
    echo "--------------------------------------------------"
done

echo "=================================================="
echo "📊 Consolidando métricas e gerando Mosaico de Gráficos OMP..."
python3 generate_report.py

echo "✅ Bateria de testes OpenMP finalizada com sucesso!"