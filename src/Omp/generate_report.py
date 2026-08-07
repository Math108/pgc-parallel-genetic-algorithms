import os
import glob
import re
import math
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from datetime import datetime
import numpy as np

# 1. Busca os relatórios individuais gerados pelo OMP.c
report_files = glob.glob("Graphs/problem_*/relatorio_execucao.txt")

sucessos = 0
falhas = 0
soma_geracoes_sucesso = 0
soma_fitness_falha = 0
soma_tempo_total = 0
tempos = []
hiperparametros_str = "Hiperparâmetros não detectados no laudo."

# 2. Data Mining via Expressões Regulares (Regex)
for idx, path in enumerate(report_files):
    with open(path, 'r', encoding='utf-8') as f:
        texto = f.read()

    # Captura os hiperparâmetros direto do cabeçalho do primeiro arquivo encontrado
    if idx == 0:
        match_p1 = re.search(r"Pop\. Total: (\d+) \| Ilhas: (\d+) \| Max Gen: (\d+)", texto)
        match_p2 = re.search(r"Mutacao Base: ([\d\.]+) \| Freq\. Migracao: (\d+)", texto)
        if match_p1 and match_p2:
            hiperparametros_str = (
                f"População Total: {match_p1.group(1)} | Threads (Ilhas): {match_p1.group(2)} | Limite de Gerações: {match_p1.group(3)}\n"
                f"Taxa de Mutação Base: {match_p2.group(1)} | Intervalo de Migração: {match_p2.group(2)} ger."
            )

    fit = int(re.search(r"Fitness Final \(Erros\): (\d+)", texto).group(1))
    gen = int(re.search(r"Geracoes Executadas: (\d+)", texto).group(1))
    tempo = float(re.search(r"Tempo Total de CPU: ([\d\.]+) segundos", texto).group(1))

    soma_tempo_total += tempo
    tempos.append(tempo)

    if fit == 0:
        sucessos += 1
        soma_geracoes_sucesso += gen
    else:
        falhas += 1
        soma_fitness_falha += fit

# 3. Processamento Estatístico
total_problemas = sucessos + falhas
if total_problemas == 0:
    print("❌ Erro: Nenhum laudo de execução encontrado em Omp/Graphs/.")
    exit()

taxa_sucesso = (sucessos / total_problemas) * 100
media_gen_sucesso = (soma_geracoes_sucesso / sucessos) if sucessos > 0 else 0
media_fit_falha = (soma_fitness_falha / falhas) if falhas > 0 else 0
media_tempo = soma_tempo_total / total_problemas
tempo_max = max(tempos)
tempo_min = min(tempos)

# =================================================================
# 4. GERAÇÃO DA SUBPASTA EXCLUSIVA DO REPORT OMP
# =================================================================
timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
bateria_dir = f"Reports/Report_OMP_{timestamp}"
os.makedirs(bateria_dir, exist_ok=True)

report_path = f"{bateria_dir}/Dashboard_Master_OMP.txt"

with open(report_path, 'w', encoding='utf-8') as f:
    f.write("==================================================\n")
    f.write("     REPORT PARALELO - OPENMP GA SUDOKU\n")
    f.write("==================================================\n")
    f.write(f"Data da Execução: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}\n")
    f.write("--------------------------------------------------\n")
    f.write(" HIPERPARÂMETROS\n")
    f.write("--------------------------------------------------\n")
    f.write(f"{hiperparametros_str}\n")
    f.write("==================================================\n")
    f.write(f"Total de Sudokus Processados:   {total_problemas}\n")
    f.write(f"Soluções Encontradas (0 Erros): {sucessos} ({taxa_sucesso:.1f}%)\n")
    f.write(f"Estagnações (Mínimo Local):    {falhas}\n")
    f.write("--------------------------------------------------\n")
    f.write(f"Média de Gerações p/ Vitória:  {media_gen_sucesso:.1f} gerações\n")
    f.write(f"Média de Erros nas Estagnações: {media_fit_falha:.2f} erros residuais\n")
    f.write("--------------------------------------------------\n")
    f.write(f"Tempo Médio de CPU por Kernel:  {media_tempo:.3f} segundos\n")
    f.write(f"Kernel Mais Veloz:              {tempo_min:.3f} segundos\n")
    f.write(f"Kernel Mais Lento:              {tempo_max:.3f} segundos\n")
    f.write(f"TEMPO TOTAL (OMP):              {soma_tempo_total:.3f} segundos\n")
    f.write("==================================================\n")

# 5. CONSTRUÇÃO DO MOSAICO DE IMAGENS PAGINADO
image_files = glob.glob("Graphs/problem_*/grafico_*.png")
if image_files:
    MAX_POR_PAGINA = 15
    total_imagens = len(image_files)
    paginas = math.ceil(total_imagens / MAX_POR_PAGINA)
    
    for p in range(paginas):
        lote = image_files[p * MAX_POR_PAGINA : (p + 1) * MAX_POR_PAGINA]
        n = len(lote)
        
        cols = 5 if n >= 5 else n
        rows = math.ceil(n / cols)
        
        fig, axes = plt.subplots(rows, cols, figsize=(cols * 4.5, rows * 3.5))
        
        if n > 1:
            axes = axes.flatten()
        else:
            axes = [axes]

        for i, img_path in enumerate(lote):
            img = mpimg.imread(img_path)
            axes[i].imshow(img)
            axes[i].axis('off')

        for j in range(i + 1, len(axes)):
            axes[j].axis('off')

        plt.tight_layout()
        
        if paginas == 1:
            img_path = f"{bateria_dir}/Painel_Graficos_OMP.png"
        else:
            img_path = f"{bateria_dir}/Painel_Graficos_OMP_Pag_{p+1}.png"
            
        plt.savefig(img_path, dpi=200, bbox_inches='tight')
        plt.close(fig) 
        
print(f"📊 Dashboard Mestre salvo com sucesso em: {bateria_dir}")