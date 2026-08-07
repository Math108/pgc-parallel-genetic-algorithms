#!/bin/bash

cd "$(dirname "$0")"

if [ -z "$1" ]; then
    echo "⚠️ Uso correto: ./run_cuda_tests.sh <quantidade>"
    exit 1
fi

TOTAL=$1

GRAPHS_DIR="Graphs"
if [ -d "$GRAPHS_DIR" ]; then
    echo -e "Limpando execucoes anteriores..."
    rm -rf "$GRAPHS_DIR"/*
else
    mkdir -p "$GRAPHS_DIR"
fi

OUTPUT_DIR="output"
mkdir -p "$OUTPUT_DIR"

echo "⚙️  Compilando Cuda.cu com otimizacao maxima (-O3 e nvcc)..."
nvcc -O3 Cuda.cu -o "$OUTPUT_DIR/cuda_solver"
#nvcc -O3 skeleton.cu -o "$OUTPUT_DIR/skeleton"

if [ $? -ne 0 ]; then
    echo "❌ Erro na compilação CUDA. Abortando."
    exit 1
fi

echo "🚀 Iniciando bateria CUDA de $TOTAL execuções..."
echo "=================================================="

for i in $(seq 1 $TOTAL); do
    
    LINHA=$(shuf -i 1-3000000 -n 1)
    
    echo "▶️  Execução $i de $TOTAL | Sorteada a linha: $LINHA no CSV"
    
    PASTA_DESTINO="$GRAPHS_DIR/problem_$LINHA"
    mkdir -p "$PASTA_DESTINO"
    
    START=$(date +%s.%N)
    
    ./$OUTPUT_DIR/cuda_solver $LINHA 1 
    #./$OUTPUT_DIR/skeleton $LINHA 1 
    
    python3 plot_fitness.py $LINHA
    
    END=$(date +%s.%N)
    TEMPO=$(awk "BEGIN {print $END - $START}")
    printf "⏱️ Tempo absoluto: %.3f segundos\n" "$TEMPO"
    
    echo "--------------------------------------------------"
done

echo "=================================================="
echo "📊 Consolidando dados e gerando Painel Visual CUDA..."
python3 generate_report_cuda.py

echo "✅ Bateria de testes CUDA finalizada com sucesso!"