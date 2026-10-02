#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "../../config.h"

/*
#define MAX_GENERATIONS 2000
#define POP_SIZE 192000
#define ISLANDS 384
#define ISLAND_POP_SIZE (POP_SIZE / ISLANDS)

#define TOURNAMENT_SIZE (int)(ISLAND_POP_SIZE * 0.20)

#define MUTATION_RATE 0.050
#define MUTATION_GROWTH 0.00050
#define MUTATION_THRESHOLD 0.150

#define MIGRATION_FREQUENCY 150
#define MIGRATION_SIZE (int)(ISLAND_POP_SIZE * 0.02)
*/

typedef struct {
    int id;
    int puzzle[81];
    int solution[81];
    int clues;
    float difficulty;
} Sudoku;

typedef struct {
    int cromossome[81];
    int fitness;
} Individual;

void printIndividual(Individual ind) {
    for (int i = 0; i < 81; i++) {
        printf("%d ", ind.cromossome[i]);
        if ((i + 1) % 9 == 0) printf("\n");
    }
}

void printHighlightedSolution(Individual ind) {
    int errorsMap[81] = {0};

    for (int col = 0; col < 9; col++) {
        int counts[10] = {0};
        for (int row = 0; row < 9; row++) {
            counts[ind.cromossome[row * 9 + col]]++;
        }
        for (int row = 0; row < 9; row++) {
            if (counts[ind.cromossome[row * 9 + col]] > 1) {
                errorsMap[row * 9 + col] = 1;
            }
        }
    }

    for (int block = 0; block < 9; block++) {
        int counts[10] = {0};
        int startRow = (block / 3) * 3;
        int startCol = (block % 3) * 3;

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                counts[ind.cromossome[(startRow + i) * 9 + (startCol + j)]]++;
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (counts[ind.cromossome[(startRow + i) * 9 + (startCol + j)]] > 1) {
                    errorsMap[(startRow + i) * 9 + (startCol + j)] = 1; 
                }
            }
        }
    }

    for (int i = 0; i < 81; i++) {
        if (errorsMap[i] == 1) {
            printf("\033[1;31m%d\033[0m ", ind.cromossome[i]);
        } else {
            printf("%d ", ind.cromossome[i]);
        }
        if ((i + 1) % 9 == 0) printf("\n");
    }
}

int calculateFitnessNoLines(Individual island[]) {
    int islandBest = 999;
    int bestIdx = -1;

    for(int i = 0; i < ISLAND_POP_SIZE; i++) {
        int mistakes = 0;

        for (int j = 0; j < 9; j++) {
            int colValues[9] = {0};
            int boxValues[9] = {0};

            int boxRow = (j / 3) * 3;
            int boxCol = (j % 3) * 3;

            for (int idx = 0; idx < 9; idx++) {
                colValues[island[i].cromossome[idx * 9 + j] - 1]++;
                boxValues[island[i].cromossome[(boxRow + idx / 3) * 9 + (boxCol + idx % 3)] - 1]++;
            }

            for (int k = 0; k < 9; k++) {
                if (colValues[k] > 1) mistakes += colValues[k] - 1;
                if (boxValues[k] > 1) mistakes += boxValues[k] - 1;
            }
        }

        island[i].fitness = mistakes;
        if (mistakes < islandBest) {
            islandBest = mistakes;
            bestIdx = i;
        }
    }

    return bestIdx;
}

//Funcao de torneio com chance de zebras
void doTournamentV2(Individual island[], int deleted[], int eliteFit, unsigned int *seed) {
    for (int i = 0; i < TOURNAMENT_SIZE; i++) {
        int fighter1, fighter2;
        fighter1 = rand_r(seed) % ISLAND_POP_SIZE;
        fighter2 = rand_r(seed) % ISLAND_POP_SIZE;

        while(deleted[fighter1]) {
            fighter1 = rand_r(seed) % ISLAND_POP_SIZE;
        }

        while (fighter1 == fighter2 || deleted[fighter2]) {
            fighter2 = rand_r(seed) % ISLAND_POP_SIZE;
        }

        int best = (island[fighter1].fitness < island[fighter2].fitness) ? fighter1 : fighter2;
        int worst = (fighter1 == best) ? fighter2 : fighter1;
        int diff = island[worst].fitness - island[best].fitness;

        if (diff == 0) {
            deleted[worst] = 1;
            continue;
        }

        float upsetChance;
        if (island[best].fitness <= eliteFit) {
            upsetChance = 0.0f;
        } else {
            if (diff <= 2) upsetChance = 0.50f;
            else if (diff <= 4) upsetChance = 0.25f;
            else if (diff <= 8) upsetChance = 0.10f;
            else upsetChance = 0.01f;
        }

        if (((float)rand_r(seed) / RAND_MAX) < upsetChance) {
            deleted[best] = 1;
        } else {
            deleted[worst] = 1;
        }
    }
}

//Funcao de crossover simples trocando linhas
void doCrossoverNoLines(Individual island[], int deleted[], unsigned int *seed) {
    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if (deleted[i]) {
            int p1 = rand_r(seed) % ISLAND_POP_SIZE;
            while (deleted[p1]) p1 = rand_r(seed) % ISLAND_POP_SIZE;

            int p2 = rand_r(seed) % ISLAND_POP_SIZE;
            while (p1 == p2 || deleted[p2]) p2 = rand_r(seed) % ISLAND_POP_SIZE;

            for (int j = 0; j < 9; j++) {
                if (rand_r(seed) % 2 == 0) {
                    for (int k = 0; k < 9; k++) island[i].cromossome[j*9 + k] = island[p1].cromossome[j*9 + k];
                } else {
                    for (int k = 0; k < 9; k++) island[i].cromossome[j*9 + k] = island[p2].cromossome[j*9 + k];
                }
            }
        }
    }
}

//Funcao de mutacao simples trocando linhas
void doMutationNoLines(Individual island[], int cluesMask[], int deleted[], int noImprovement, unsigned int *seed) {
    float mutationChance = MUTATION_RATE + (MUTATION_GROWTH * noImprovement);
    if (mutationChance > MUTATION_THRESHOLD) mutationChance = MUTATION_THRESHOLD;

    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if(deleted[i]) {
            for (int j = 0; j < 9; j++) {
                if (((float)rand_r(seed) / RAND_MAX) < mutationChance) {

                    int freeCells[9];
                    int freeCount = 0; 

                    for(int col = 0; col < 9; col++) {
                        if(cluesMask[j * 9 + col] == 0) {
                            freeCells[freeCount] = col;
                            freeCount++;
                        }
                    }

                    if (freeCount >= 2) {
                        int idx1 = rand() % freeCount; 
                        int idx2 = rand() % freeCount;
                        while(idx1 == idx2) {
                            idx2 = rand() % freeCount;
                        }
                        
                        int c1 = freeCells[idx1];
                        int c2 = freeCells[idx2];
                        
                        int tmp = island[i].cromossome[j * 9 + c1];
                        island[i].cromossome[j * 9 + c1] = island[i].cromossome[j * 9 + c2];
                        island[i].cromossome[j * 9 + c2] = tmp;
                    }
                }
            }
        }
    }
}

/* Funcao de mutacao com heuristica
*  Verifica se a celula a ser mudada possui erros nas colunas e boxes 3x3,
*  em caso positivo, ela eh priorizada para a mutacao
*/
void doMutationV2(Individual island[], int cluesMask[], int deleted[], int noImprovement, unsigned int *seed) {
    float mutationChance = MUTATION_RATE + (MUTATION_GROWTH * noImprovement);
    if (mutationChance > MUTATION_THRESHOLD) mutationChance = MUTATION_THRESHOLD;

    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if(deleted[i]) {
            for (int row = 0; row < 9; row++) {
                if (((float)rand_r(seed) / RAND_MAX) < mutationChance) {
                    
                    int errorCells[9]; 
                    int errorCount = 0;
                    int perfectCells[9]; 
                    int perfectCount = 0;

                    for(int col = 0; col < 9; col++) {
                        if(cluesMask[row * 9 + col] == 0) {
                            int val = island[i].cromossome[row * 9 + col];
                            int hasError = 0;

                            for (int r = 0; r < 9; r++) {
                                if (r != row && island[i].cromossome[r * 9 + col] == val) {
                                    hasError = 1;
                                    break;
                                }
                            }

                            if (!hasError) {
                                int startRow = (row / 3) * 3;
                                int startCol = (col / 3) * 3;
                                for (int r = 0; r < 3; r++) {
                                    for (int c = 0; c < 3; c++) {
                                        int actualRow = startRow + r;
                                        int actualCol = startCol + c;
                                        if (actualRow != row && actualCol != col && island[i].cromossome[actualRow * 9 + actualCol] == val) {
                                            hasError = 1;
                                            r = 3; 
                                            break;
                                        }
                                    }
                                }
                            }

                            if (hasError) errorCells[errorCount++] = col;
                            else perfectCells[perfectCount++] = col;
                        }
                    }

                    if (errorCount > 0) {
                        int c1 = errorCells[rand_r(seed) % errorCount];
                        int c2;

                        if (errorCount > 1 && ((float)rand_r(seed) / RAND_MAX) < 0.70) {
                            c2 = errorCells[rand_r(seed) % errorCount];
                            while(c1 == c2) c2 = errorCells[rand_r(seed) % errorCount];
                        } 
                        else if (perfectCount > 0) {
                            c2 = perfectCells[rand_r(seed) % perfectCount];
                        } 
                        else continue; 

                        int tmp = island[i].cromossome[row * 9 + c1];
                        island[i].cromossome[row * 9 + c1] = island[i].cromossome[row * 9 + c2];
                        island[i].cromossome[row * 9 + c2] = tmp;

                    } 
                    else if (perfectCount >= 2 && noImprovement > 200) {
                        int c1 = perfectCells[rand_r(seed) % perfectCount];
                        int c2 = perfectCells[rand_r(seed) % perfectCount];
                        while(c1 == c2) c2 = perfectCells[rand_r(seed) % perfectCount];
                        
                        int tmp = island[i].cromossome[row * 9 + c1];
                        island[i].cromossome[row * 9 + c1] = island[i].cromossome[row * 9 + c2];
                        island[i].cromossome[row * 9 + c2] = tmp;
                    }
                }
            }
        }
    }
}

//Funcao de migração (nao usada na versao omp)
void doMigration(Individual (*population)[ISLAND_POP_SIZE]) {
    Individual bestIndividuals[ISLANDS][MIGRATION_SIZE];
    int worstIndividuals[ISLANDS][MIGRATION_SIZE];

    for (int i = 0; i < ISLANDS; i++) {
        int selectedBest[ISLAND_POP_SIZE] = {0};
        int selectedWorst[ISLAND_POP_SIZE] = {0};

        for (int m = 0; m < MIGRATION_SIZE; m++) {
            int bestIdx = -1;
            int worstIdx = -1;

            for (int j = 0; j < ISLAND_POP_SIZE; j++) {
                if (!selectedBest[j]) {
                    if (bestIdx == -1 || population[i][j].fitness < population[i][bestIdx].fitness) {
                        bestIdx = j;
                    }
                }
                if (!selectedWorst[j]) {
                    if (worstIdx == -1 || population[i][j].fitness > population[i][worstIdx].fitness) {
                        worstIdx = j;
                    }
                }
            }

            selectedBest[bestIdx] = 1;
            selectedWorst[worstIdx] = 1;

            bestIndividuals[i][m] = population[i][bestIdx];
            worstIndividuals[i][m] = worstIdx;
        }
    }

    for (int i = 0; i < ISLANDS; i++) {
        int nextIsland = (i + 1) % ISLANDS;
        for (int m = 0; m < MIGRATION_SIZE; m++) {
            population[nextIsland][worstIndividuals[nextIsland][m]] = bestIndividuals[i][m];
        }
    }
}

void getTabuleiro(const char* strTab, int tabuleiro[81]) {
    for (int i = 0; i < 81; i++) {
        if (strTab[i] == '.') tabuleiro[i] = 0;
        else tabuleiro[i] = strTab[i] - '0';
    }
}

void geneticAlgorithm(int tabuleiro[81], int cluesMask[81], int silentMode, int problemLine, int runId) {
    Individual (*population)[ISLAND_POP_SIZE] = malloc(ISLANDS * sizeof(*population));
    if (!population) {
        printf("Falha ao alocar populacao\n");
        exit(1);
    }

    int globalBest = 999;
    volatile int stopFlag = 0;
    int lastGenGlobal = 0;
    
    char filename[128];
    sprintf(filename, "Graphs/problem_%d_run_%d/fitness_log_linha_%d.csv", problemLine, runId, problemLine);
    FILE *logFile = fopen(filename, "w");
    if (logFile) fprintf(logFile, "Geracao,MelhorGlobal,MelhorGeracao\n");

    Individual bestSolution;
    bestSolution.fitness = 999;

    omp_lock_t mailboxLocks[ISLANDS];
    int mailboxFull[ISLANDS];
    Individual **mailbox = malloc(ISLANDS * sizeof(Individual*));
    for (int i = 0; i < ISLANDS; i++) {
        mailbox[i] = malloc(MIGRATION_SIZE * sizeof(Individual));
        if (!mailbox[i]) {
            printf("Erro ao alocar meomria das mailboxes\n");
            exit(1);
        }
    }

    for (int i = 0; i < ISLANDS; i++) {
        omp_init_lock(&mailboxLocks[i]);
        mailboxFull[i] = 0;
    }

    double start_time = omp_get_wtime();

    #pragma omp parallel num_threads(ISLANDS)
    {
        int myIsland = omp_get_thread_num();
        unsigned int seed = time(NULL) ^ myIsland;

        int myBestHistory = 999;
        int myNoImprovement = 0;
        int localLastGen = 0;

        //Inicialização aleatória dos indivuos
        for (int j = 0; j < ISLAND_POP_SIZE; j++) {
            population[myIsland][j].fitness = 0;
            for (int row = 0; row < 9; row++) {
                int available[9];       
                int availableCount = 0;
                int used[10] = {0};
                for (int col = 0; col < 9; col++) {
                    int index = row * 9 + col;
                    if (cluesMask[index] == 1) {
                        population[myIsland][j].cromossome[index] = tabuleiro[index];
                        used[tabuleiro[index]] = 1;
                    }
                }
                for (int num = 1; num <= 9; num++) {
                    if (!used[num]) available[availableCount++] = num;
                }
                for (int k = availableCount - 1; k > 0; k--) {
                    int r = rand_r(&seed) % (k + 1);
                    int temp = available[k];
                    available[k] = available[r];
                    available[r] = temp;
                }
                int fillIdx = 0;
                for (int col = 0; col < 9; col++) {
                    int index = row * 9 + col;
                    if (cluesMask[index] == 0) population[myIsland][j].cromossome[index] = available[fillIdx++];
                }
            }
        }

        Individual myBestSolution;
        myBestSolution.fitness = 999;

        //Executa as geracoes
        for (int generation = 0; generation < MAX_GENERATIONS && !stopFlag; generation++) {
            localLastGen = generation;
            
            int bestIdx = -1;
            int bestFitness = 999;
            int worstIdx = -1;
            int worstFitness = -1;

            for (int j = 0; j < ISLAND_POP_SIZE; j++) {
                int mistakes = 0;
                for (int r = 0; r < 9; r++) {
                    int colValues[9] = {0};
                    int boxValues[9] = {0};
                    int boxRow = (r / 3) * 3;
                    int boxCol = (r % 3) * 3;
                    for (int idx = 0; idx < 9; idx++) {
                        colValues[population[myIsland][j].cromossome[idx * 9 + r] - 1]++;
                        boxValues[population[myIsland][j].cromossome[(boxRow + idx / 3) * 9 + (boxCol + idx % 3)] - 1]++;
                    }
                    for (int k = 0; k < 9; k++) {
                        if (colValues[k] > 1) mistakes += colValues[k] - 1;
                        if (boxValues[k] > 1) mistakes += boxValues[k] - 1;
                    }
                }
                population[myIsland][j].fitness = mistakes;
                if (mistakes < bestFitness) { bestFitness = mistakes; bestIdx = j; }
                if (mistakes > worstFitness) { worstFitness = mistakes; worstIdx = j; }
            }

            if (bestFitness < myBestHistory) {
                myBestHistory = bestFitness;
                myNoImprovement = 0;
                myBestSolution = population[myIsland][bestIdx]; 
            } else {
                myNoImprovement++;
            }

            if ((generation > 0 && generation % MIGRATION_FREQUENCY == 0) || generation == MAX_GENERATIONS - 1) {
                #pragma omp critical
                {
                    if (myBestHistory < globalBest) {
                        globalBest = myBestHistory;
                        bestSolution = myBestSolution;
                        if (globalBest == 0) stopFlag = 1;
                    }
                }
                
                if (myIsland == 0 && logFile) {
                    fprintf(logFile, "%d,%d,%d\n", generation, globalBest, bestFitness);
                    fflush(logFile);
                }
            }
            
            if (stopFlag) break;

            //Realiza migracao assincrona
            if (generation > 0 && generation % MIGRATION_FREQUENCY == 0) {
                int nextIsland = (myIsland + 1) % ISLANDS;
                
                //Seleciona os migrantes atraves de um torneio
                if (omp_test_lock(&mailboxLocks[nextIsland])) {
                    if (mailboxFull[nextIsland] == 0) {
                        
                        for (int m = 0; m < MIGRATION_SIZE; m++) {
                            int fighterA = rand_r(&seed) % ISLAND_POP_SIZE;
                            int fighterB = rand_r(&seed) % ISLAND_POP_SIZE;
                            int fighterC = rand_r(&seed) % ISLAND_POP_SIZE;

                            int bIdx = fighterA;
                            if (population[myIsland][fighterB].fitness < population[myIsland][bIdx].fitness) {
                                bIdx = fighterB;
                            }
                            if (population[myIsland][fighterC].fitness < population[myIsland][bIdx].fitness) {
                                bIdx = fighterC;
                            }

                            //Envia o vencedor do torneio para a ilha vizinha
                            mailbox[nextIsland][m] = population[myIsland][bIdx]; 
                        }
                        mailboxFull[nextIsland] = 1;
                    }
                    omp_unset_lock(&mailboxLocks[nextIsland]);
                }
            }
            //Recebe os migrantes atraves de um torneio
            if (mailboxFull[myIsland] == 1) {
                if (omp_test_lock(&mailboxLocks[myIsland])) {
                    if (mailboxFull[myIsland] == 1) {

                        for (int m = 0; m < MIGRATION_SIZE; m++) {
                            int loserA = rand_r(&seed) % ISLAND_POP_SIZE;
                            int loserB = rand_r(&seed) % ISLAND_POP_SIZE;
                            int loserC = rand_r(&seed) % ISLAND_POP_SIZE;

                            int wIdx = loserA;
                            if (population[myIsland][loserB].fitness > population[myIsland][wIdx].fitness) {
                                wIdx = loserB;
                            }
                            if (population[myIsland][loserC].fitness > population[myIsland][wIdx].fitness) {
                                wIdx = loserC;
                            }

                            population[myIsland][wIdx] = mailbox[myIsland][m];
                        }
                        mailboxFull[myIsland] = 0;
                    }
                    omp_unset_lock(&mailboxLocks[myIsland]);
                }
            }
            int deleted[ISLAND_POP_SIZE] = {0};
            int eliteFit = bestFitness;
            
            doTournamentV2(population[myIsland], deleted, eliteFit, &seed);
            doCrossoverNoLines(population[myIsland], deleted, &seed);
            //doMutationNoLines(population[myIsland], cluesMask, deleted, myNoImprovement, &seed);
            doMutationV2(population[myIsland], cluesMask, deleted, myNoImprovement, &seed);
        }
        
        #pragma omp critical
        {
            if (localLastGen > lastGenGlobal) lastGenGlobal = localLastGen;
        }
    }

    for (int i = 0; i < ISLANDS; i++) {
        omp_destroy_lock(&mailboxLocks[i]);
    }

    double end_time = omp_get_wtime();
    double time_spent = end_time - start_time;
    int hours = (int)(time_spent / 3600);
    int minutes = ((int)time_spent % 3600) / 60;
    double seconds = time_spent - (hours * 3600) - (minutes * 60);
    
    if (logFile) fclose(logFile);

    char reportName[128];
    sprintf(reportName, "Graphs/problem_%d_run_%d/relatorio_execucao.txt", problemLine, runId);
    FILE *reportFile = fopen(reportName, "w");
    
    if (reportFile) {
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, " RELATORIO TECNICO (ASSINCRONO) - SUDOKU LINHA %d\n", problemLine);
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, " Pop. Total: %d | Ilhas: %d | Max Gen: %d\n", POP_SIZE, ISLANDS, MAX_GENERATIONS);
        fprintf(reportFile, " Mutacao Base: %.3f | Freq. Migracao: %d\n", MUTATION_RATE, MIGRATION_FREQUENCY);
        fprintf(reportFile, "==================================================\n");
        
        if (globalBest == 0) fprintf(reportFile, "STATUS: RESOLVIDO COM SUCESSO!\n");
        else fprintf(reportFile, "STATUS: LIMITE DE GERACOES ATINGIDO (ESTAGNADO)\n");
        
        fprintf(reportFile, "Fitness Final (Erros): %d\n", globalBest);
        fprintf(reportFile, "Geracoes Executadas: %d / %d\n", lastGenGlobal, MAX_GENERATIONS);
        fprintf(reportFile, "Tempo Total de CPU: %.3f segundos\n", time_spent);
        fprintf(reportFile, "==================================================\n");
        for (int i = 0; i < 81; i++) {
            fprintf(reportFile, "%d ", bestSolution.cromossome[i]);
            if ((i + 1) % 9 == 0) fprintf(reportFile, "\n");
        }
        fclose(reportFile);
    }

    if(!silentMode) {
        printf("Melhor fitness encontrado: %d.\n", globalBest);
        printf("Geracoes concluidas (Max): %d.\n", lastGenGlobal);
        printf("Tempo total de processamento: %02d horas, %02d minutos e %.2f segundos\n", hours, minutes, seconds);

        if(globalBest == 0) {
            printf("\n==================================================\n");
            printf("MELHOR SOLUCAO ENCONTRADA (%d erros):\n", bestSolution.fitness);
            printHighlightedSolution(bestSolution);
            printf("==================================================\n");
        }
    }

    for (int i = 0; i < ISLANDS; i++) {
        free(mailbox[i]);
    }
    free(mailbox);
    free(population);
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("../../data/sudoku-3m.csv", "r");
    
    if(!file) {
        file = fopen("../data/sudoku-3m.csv", "r");
    }
    
    if(!file) {
        file = fopen("data/sudoku-3m.csv", "r");
    }

    if(!file) {
        printf("Falha ao abrir o arquivo\n");
        return 1;
    }

    char line[256];
    int problemLine;
    int silentMode = 0;

    if (argc > 1) problemLine = atoi(argv[1]);
    else {
        srand(time(NULL));
        problemLine = (rand() % 3000000) + 1;
    }

    if (argc > 2) silentMode = atoi(argv[2]);

    int runId = 1;
    if (argc > 3) {
        runId = atoi(argv[3]);
    }

    int currentLine = 0;

    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Falha ao ler o CSV\n");
        return 1;
    }

    while (fgets(line, sizeof(line), file)) {
        currentLine++;
        if (currentLine == problemLine) {
            char* str_id = strtok(line, ",");
            char* str_puzzle = strtok(NULL, ",");
            char* str_solution = strtok(NULL, ",");
            char* str_clues = strtok(NULL, ",");
            char* str_difficulty = strtok(NULL, "\n");

            Sudoku sudoku;
            sudoku.id = atoi(str_id);
            getTabuleiro(str_puzzle, sudoku.puzzle);
            getTabuleiro(str_solution, sudoku.solution);
            sudoku.clues = atoi(str_clues);
            sudoku.difficulty = atof(str_difficulty);

            int cluesMask[81] = {0};
            for (int i = 0; i < 81; i++) {
                if (sudoku.puzzle[i] != 0) cluesMask[i] = 1;
            }

            printf("Iniciando algoritmo genetico com OpenMP...\n");
            geneticAlgorithm(sudoku.puzzle, cluesMask, silentMode, problemLine, runId);
        }
    }

    fclose(file);
    return 0;
}