import os
import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime

stats = {}
global_report_dir = "Reports/Relatorio_Global"
os.makedirs(global_report_dir, exist_ok=True)

try:
    with open('recent_reports.txt', 'r', encoding='utf-8') as f:
        lines = f.readlines()
except FileNotFoundError:
    print("❌ Erro: O arquivo 'recent_reports.txt' não foi encontrado. Execute o run_master.sh primeiro.")
    exit()

for line in lines:
    if not line.strip(): continue
    path, exec_name = line.strip().split('|')
    
    if not os.path.exists(path):
        continue

    with open(path, 'r', encoding='utf-8') as f:
        texto = f.read()
    
    # Mineração Regex robusta (imune a erros de acentuação do C vs Python)
    match_total = re.search(r"(?:Total de Sudokus Processados|Total de Problemas Processados):\s+(\d+)", texto)
    match_sucesso = re.search(r"Solu[cç][oõ]es Encontradas \(0 Erros\):\s+(\d+)\s+\(([\d\.]+)%\)", texto)
    match_falha = re.search(r"(?:Estagna[cç][oõ]es|Falhas)[^:]*:\s+(\d+)", texto)
    match_gen = re.search(r"M[eé]dia de Gera[cç][oõ]es p/ (?:Vit[oó]ria|Sucesso):\s+([\d\.]+)", texto)
    match_fit = re.search(r"M[eé]dia de Erros nas (?:Estagna[cç][oõ]es|Falhas):\s+([\d\.]+)", texto)
    match_tempo_medio = re.search(r"Tempo.*M[eé]dio.*:\s+([\d\.]+) segundos", texto, re.IGNORECASE)
    match_tempo_min = re.search(r"(?:Tempo do Mais R[aá]pido|Tempo do Mais Rapido|Kernel Mais Veloz):\s+([\d\.]+) segundos", texto, re.IGNORECASE)
    match_tempo_max = re.search(r"(?:Tempo do Mais Lento|Kernel Mais Lento):\s+([\d\.]+) segundos", texto, re.IGNORECASE)
    match_tempo_total = re.search(r"TEMPO TOTAL.*:\s+([\d\.]+) segundos", texto, re.IGNORECASE)

    if match_total and match_sucesso:
        stats[exec_name] = {
            'total': int(match_total.group(1)),
            'sucessos': int(match_sucesso.group(1)),
            'taxa_sucesso': float(match_sucesso.group(2)),
            'falhas': int(match_falha.group(1)) if match_falha else 0,
            'media_gen': float(match_gen.group(1)) if match_gen else 0.0,
            'media_fit': float(match_fit.group(1)) if match_fit else 0.0,
            'tempo_medio': float(match_tempo_medio.group(1)) if match_tempo_medio else 0.0,
            'tempo_min': float(match_tempo_min.group(1)) if match_tempo_min else 0.0,
            'tempo_max': float(match_tempo_max.group(1)) if match_tempo_max else 0.0,
            'tempo_total': float(match_tempo_total.group(1)) if match_tempo_total else 0.0
        }

if not stats:
    print("❌ Nenhum dado consolidado para exibir. Verifique se as execuções locais foram bem sucedidas.")
    exit()

# Geração do Relatório Técnico em Texto
timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
report_path = f"{global_report_dir}/Report_Consolidado_Global_{timestamp}.txt"

with open(report_path, 'w', encoding='utf-8') as f:
    f.write("==================================================\n")
    f.write("    RELATORIO CONSOLIDADO - COMPARATIVO GLOBAL\n")
    f.write("==================================================\n")
    f.write(f"Data da Execucao: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}\n")
    f.write("==================================================\n\n")
    
    for arq, data in stats.items():
        f.write(f"[{arq.upper()}]\n")
        f.write(f"--------------------------------------------------\n")
        f.write(f"Total de Sudokus Processados:   {data['total']}\n")
        f.write(f"Solucoes Encontradas (0 Erros): {data['sucessos']} ({data['taxa_sucesso']:.1f}%)\n")
        f.write(f"Falhas / Estagnacoes:         {data['falhas']}\n")
        f.write(f"Media de Geracoes p/ Sucesso:   {data['media_gen']:.1f} geracoes\n")
        f.write(f"Media de Erros nas Falhas:      {data['media_fit']:.2f} erros residuais\n")
        f.write(f"Tempo Medio por Sudoku:         {data['tempo_medio']:.3f} segundos\n")
        f.write(f"Tempo do Mais Rapido:           {data['tempo_min']:.3f} segundos\n")
        f.write(f"Tempo do Mais Lento:            {data['tempo_max']:.3f} segundos\n")
        f.write(f"TEMPO TOTAL DA BATERIA:         {data['tempo_total']:.3f} segundos\n")
        f.write("==================================================\n\n")

print(f"📄 Relatório Técnico Consolidado gerado em: {report_path}")

# Geração do Gráfico Dinâmico
labels = list(stats.keys())
sucessos = [stats[a]['taxa_sucesso'] for a in labels]
tempos = [stats[a]['tempo_medio'] for a in labels]

x = np.arange(len(labels))
width = 0.35

fig, ax1 = plt.subplots(figsize=(12, 7))

color = 'tab:blue'
ax1.set_ylabel('Tempo Médio por Sudoku (s)', color=color, fontweight='bold')
bars1 = ax1.bar(x - width/2, tempos, width, label='Tempo Médio', color=color)
ax1.tick_params(axis='y', labelcolor=color)

ax2 = ax1.twinx()
color = 'tab:green'
ax2.set_ylabel('Taxa de Sucesso (%)', color=color, fontweight='bold')
bars2 = ax2.bar(x + width/2, sucessos, width, label='Sucesso (%)', color=color)
ax2.tick_params(axis='y', labelcolor=color)
ax2.set_ylim(0, 110)

ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontweight='bold', rotation=45, ha='right')
plt.title('Comparativo Evolutivo de Performance: Baseline vs CUDA_Vx', fontsize=14, fontweight='bold')

for bar in bars1:
    ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f'{bar.get_height():.2f}s', ha='center', va='bottom', fontsize=8)
for bar in bars2:
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f'{bar.get_height():.1f}%', ha='center', va='bottom', fontsize=8)

plt.tight_layout()
grafico_path = f"{global_report_dir}/Grafico_Comparativo_Global_{timestamp}.png"
plt.savefig(grafico_path, dpi=300)
print(f"📊 Gráfico comparativo salvo em: {grafico_path}")