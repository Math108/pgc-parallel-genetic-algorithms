import pandas as pd
import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

if len(sys.argv) < 3:
    print("❌ Erro: Passe a linha do Sudoku e o ID da repetição.")
    sys.exit(1)

problem_id = sys.argv[1]
run_id = sys.argv[2]

base_path = f"Graphs/problem_{problem_id}_run_{run_id}"
csv_filename = f"{base_path}/fitness_log_linha_{problem_id}.csv"
png_filename = f"{base_path}/grafico_convergencia_{problem_id}.png"

try:
    if not os.path.exists(csv_filename):
        print(f"⚠️ Arquivo {csv_filename} não encontrado. O C falhou?")
        sys.exit(1)

    data = pd.read_csv(csv_filename)

    plt.figure(figsize=(10, 6))
    plt.plot(data['Geracao'], data['MelhorGlobal'], label='Melhor Global (Elitismo)', color='blue', linewidth=2, marker='o', markersize=6)
    plt.plot(data['Geracao'], data['MelhorGeracao'], label='Melhor da Geração', color='orange', alpha=0.5, linestyle=':', marker='x', markersize=6)

    plt.title(f'Curva de Convergência - Sudoku Linha {problem_id}', fontsize=14, fontweight='bold')
    plt.xlabel('Gerações', fontsize=12)
    plt.ylabel('Fitness (Número de Erros)', fontsize=12)
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)

    plt.tight_layout()
    # Salva o PNG direto na subpasta
    plt.savefig(png_filename, dpi=300)
    print(f"📊 Gráfico salvo: {png_filename}")

except Exception as e:
    print(f"❌ Erro ao gerar o gráfico para a linha {problem_id}. Detalhes: {e}")