import os
import glob
import re
import math
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from datetime import datetime
import numpy as np

report_files = glob.glob("Graphs/problem_*/relatorio_execucao.txt")

sucessos = 0
falhas = 0
soma_geracoes_sucesso = 0
soma_fitness_falha = 0
soma_tempo_total = 0
tempos = []
hiperparametros_str = "Hiperparâmetros não encontrados."

for idx, path in enumerate(report_files):
    with open(path, 'r', encoding='utf-8') as f:
        texto = f.read()

    if idx == 0:
        match_p1 = re.search(r"Pop\. Total: (\d+) \| Ilhas: (\d+) \| Max Gen: (\d+)", texto)
        match_p2 = re.search(r"Mutacao Base: ([\d\.]+) \| Freq\. Migracao: (\d+)", texto)
        if match_p1 and match_p2:
            hiperparametros_str = (
                f"Populacao Total: {match_p1.group(1)} | Total de Ilhas: {match_p1.group(2)} | Limite de Geracoes: {match_p1.group(3)}\n"
                f"Taxa de Mutacao: {match_p2.group(1)} | Frequencia de Migracao: {match_p2.group(2)}"
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

total_problemas = sucessos + falhas
if total_problemas == 0:
    print("❌ Nenhum laudo encontrado para gerar o relatório.")
    exit()

taxa_sucesso = (sucessos / total_problemas) * 100
media_gen_sucesso = (soma_geracoes_sucesso / sucessos) if sucessos > 0 else 0
media_fit_falha = (soma_fitness_falha / falhas) if falhas > 0 else 0
media_tempo = soma_tempo_total / total_problemas
tempo_max = max(tempos)
tempo_min = min(tempos)

timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
bateria_dir = f"Reports/Report_{timestamp}"
os.makedirs(bateria_dir, exist_ok=True)

report_path = f"{bateria_dir}/Dashboard_Master.txt"

with open(report_path, 'w', encoding='utf-8') as f:
    f.write("==================================================\n")
    f.write(" REPORT SEQUENCIAL - ALGORITMO GENETICO SUDOKU\n")
    f.write("==================================================\n")
    f.write(f"Data da Execucao: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}\n")
    f.write("--------------------------------------------------\n")
    f.write(" HIPERPARAMETROS\n")
    f.write("--------------------------------------------------\n")
    f.write(f"{hiperparametros_str}\n")
    f.write("==================================================\n")
    f.write(f"Total de Problemas Processados: {total_problemas}\n")
    f.write(f"Solucoes Encontradas (0 Erros): {sucessos} ({taxa_sucesso:.1f}%)\n")
    f.write(f"Falhas (Estagnacao no Limite):  {falhas}\n")
    f.write("--------------------------------------------------\n")
    f.write(f"Media de Geracoes p/ Sucesso:   {media_gen_sucesso:.1f} geracoes\n")
    f.write(f"Media de Erros nas Falhas:      {media_fit_falha:.2f} erros residuais\n")
    f.write("--------------------------------------------------\n")
    f.write(f"Tempo Medio por Sudoku:         {media_tempo:.3f} segundos\n")
    f.write(f"Tempo do Mais Rapido:           {tempo_min:.3f} segundos\n")
    f.write(f"Tempo do Mais Lento:            {tempo_max:.3f} segundos\n")
    f.write(f"TEMPO TOTAL DE CPU (C):         {soma_tempo_total:.3f} segundos\n")
    f.write("==================================================\n")

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
        
        if n > 1 and paginas >= 1:
            if type(axes) is not list and type(axes) is not np.ndarray:
                axes = [axes]
            else:
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
            img_path = f"{bateria_dir}/Painel_Graficos.png"
        else:
            img_path = f"{bateria_dir}/Painel_Graficos_Pag_{p+1}.png"
            
        plt.savefig(img_path, dpi=200, bbox_inches='tight')
        plt.close(fig) 
        
print(f"📊 Dashboard Mestre e Gráficos gerados com sucesso na pasta: {bateria_dir}")