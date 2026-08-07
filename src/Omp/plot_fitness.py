import sys
import os
import pandas as pd
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("⚠️ Uso: python3 plot_fitness.py <numero_da_linha>")
    sys.exit(1)

linha = sys.argv[1]
csv_path = f"Graphs/problem_{linha}/fitness_log_linha_{linha}.csv"
output_png = f"Graphs/problem_{linha}/grafico_evolucao_{linha}.png"

if not os.path.exists(csv_path):
    print(f"❌ Erro: CSV não encontrado em {csv_path}")
    sys.exit(1)

# Carrega os logs da evolução
df = pd.read_csv(csv_path)

plt.figure(figsize=(7, 4))
plt.plot(df['Geracao'], df['MelhorGlobal'], label='Melhor Histórico (Global)', color='#1f77b4', linewidth=2)
plt.plot(df['Geracao'], df['MelhorGeracao'], label='Melhor da Geração Atual', color='#ff7f0e', alpha=0.6, linestyle='--')

plt.title(f'Convergência do Algoritmo Genético (OMP) - Sudoku #{linha}', fontsize=11, fontweight='bold')
plt.xlabel('Gerações', fontsize=9)
plt.ylabel('Fitness (Erros)', fontsize=9)
plt.grid(True, linestyle=':', alpha=0.6)
plt.legend(fontsize=8, loc='upper right')

# Salva em alta resolução na pasta do problema
plt.savefig(output_png, dpi=150, bbox_inches='tight')
plt.close()