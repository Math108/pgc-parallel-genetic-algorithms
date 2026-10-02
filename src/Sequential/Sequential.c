#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#define MIGRATION_FREQUENCY 50
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

void printIsland(Individual island[]) {
    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        printf("Individuo %d (Fitness = %d):\n", i, island[i].fitness);
        printIndividual(island[i]);
        printf("\n");
    }
}

void printPopulation(Individual population[ISLANDS][ISLAND_POP_SIZE]) {
    for (int i = 0; i < ISLANDS; i++) {
        printf("Ilha %d:\n", i);
        printIsland(population[i]);
        printf("==============================\n");
    }
}

void printReport(Individual population[ISLANDS][ISLAND_POP_SIZE], int generation, int noImprovementIsland[], int globalBest) {
    printf("\n=========================================================================\n");
    printf(" RELATORIO DA GERACAO %d | RECORD GERAL: %d ERROS\n", generation, globalBest);
    printf("-------------------------------------------------------------------------\n");
    printf(" ILHA | MELHOR | PIOR  | MEDIA  | ESTAGNACAO | DIVERSIDADE (Pior-Melhor)\n");
    printf("-------------------------------------------------------------------------\n");

    for (int i = 0; i < ISLANDS; i++) {
        int best = 999;
        int worst = -1;
        long sum = 0;

        for (int j = 0; j < ISLAND_POP_SIZE; j++) {
            int fit = population[i][j].fitness;
            if (fit < best) best = fit;
            if (fit > worst) worst = fit;
            sum += fit;
        }

        float avg = (float)sum / ISLAND_POP_SIZE;
        int diversity = worst - best;

        printf("  %02d  |   %03d  |  %03d  | %06.2f |   %06d   | %d erros de diferenca\n",
               i, best, worst, avg, noImprovementIsland[i], diversity);
    }
    printf("=========================================================================\n\n");
}

void getTabuleiro(const char* strTab, int tabuleiro[81]) {
    for (int i = 0; i < 81; i++) {
        if (strTab[i] == '.') tabuleiro[i] = 0;
        else tabuleiro[i] = strTab[i] - '0';
    }
}

void printCluesMask(int cluesMask[81]) {
    printf("Mascara de casas preenchidas:\n");
    for (int i = 0; i < 81; i++) {
        printf("%d ", cluesMask[i]);
        if ((i + 1) % 9 == 0) printf("\n");
    }
}

void printSudoku(Sudoku sudoku) {
    printf("ID: %d\n", sudoku.id);
    printf("Numero de casas preenchidas: %d\n", sudoku.clues);
    printf("Dificuldade: %.2f\n", sudoku.difficulty);
    printf("Tabuleiro:\n");
    for (int i = 0; i < 81; i++) {
        printf("%d ", sudoku.puzzle[i]);
        if ((i + 1) % 9 == 0) printf("\n");
    }
    printf("Solucao:\n");
    for (int i = 0; i < 81; i++) {
        printf("%d ", sudoku.solution[i]);
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

//Calcula o fitness com base no numero de erros
int calculateFitness(Individual island[]) {
    int islandBest = 999;
    int bestIdx = -1;

    for(int i = 0; i < ISLAND_POP_SIZE; i++) {
        int mistakes = 0;

        for (int j = 0; j < 9; j++) {

            int lineValues[9] = {0};
            int colValues[9] = {0};
            int boxValues[9] = {0};

            int boxRow = (j / 3) * 3;
            int boxCol = (j % 3) * 3;

            for (int idx = 0; idx < 9; idx++) {
                lineValues[island[i].cromossome[j * 9 + idx] - 1]++;
                colValues[island[i].cromossome[idx * 9 + j] - 1]++;
                boxValues[island[i].cromossome[(boxRow + idx / 3) * 9 + (boxCol + idx % 3)] - 1]++;
            }

            for (int k = 0; k < 9; k++) {
                if (lineValues[k] > 1) mistakes += lineValues[k] - 1;
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

void printFitness(Individual island[]) {
    printf("Fitness dos individuos:\n");
    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        printf("Individuo %d: Fitness = %d\n", i, island[i].fitness);
    }
}

void printDeleted(int deleted[]) {
    printf("Individuos deletados:\n");
    for (int i = 0; i < TOURNAMENT_SIZE; i++) {
        printf("Individuo %d\n", deleted[i]);
    }
}

void doTournament(Individual island[], int deleted[]) {
    for (int i = 0; i < TOURNAMENT_SIZE; i++) {

        int fighter1, fighter2;
        fighter1 = rand() % ISLAND_POP_SIZE;
        fighter2 = rand() % ISLAND_POP_SIZE;

        while(deleted[fighter1]) {
            fighter1 = rand() % ISLAND_POP_SIZE;
        }

        while (fighter1 == fighter2 || deleted[fighter2]) {
            fighter2 = rand() % ISLAND_POP_SIZE;
        }

        //printf("Torneio %d: Individuo %d (Fitness = %d) vs Individuo %d (Fitness = %d)\n", i, fighter1, island[fighter1].fitness, fighter2, island[fighter2].fitness);
        if (island[fighter1].fitness < island[fighter2].fitness) {
            deleted[fighter2] = 1;
        } else {
            deleted[fighter1] = 1;
        }
    }
}

void doTournamentV2(Individual island[], int deleted[], int eliteFit) {
    for (int i = 0; i < TOURNAMENT_SIZE; i++) {

        int fighter1, fighter2;
        fighter1 = rand() % ISLAND_POP_SIZE;
        fighter2 = rand() % ISLAND_POP_SIZE;

        while(deleted[fighter1]) {
            fighter1 = rand() % ISLAND_POP_SIZE;
        }

        while (fighter1 == fighter2 || deleted[fighter2]) {
            fighter2 = rand() % ISLAND_POP_SIZE;
        }

        int best, worst;
        if (island[fighter1].fitness < island[fighter2].fitness) {
            best = fighter1;
            worst = fighter2;
        } else {
            best = fighter2;
            worst = fighter1;
        }

        int diff = island[worst].fitness - island[best].fitness;

        if (diff == 0) {
            deleted[worst] = 1;
            continue;
        }

        float upsetChance;

        if (island[best].fitness <= eliteFit) {
            upsetChance = 0.0f;
        } else {
            if (diff <= 2) {
                upsetChance = 0.50f;
            } else if (diff <= 4) {
                upsetChance = 0.25f;
            } else if (diff <= 8) {
                upsetChance = 0.10f;
            } else {
                upsetChance = 0.01f;
            }
        }

        if (((float)rand() / RAND_MAX) < upsetChance) {
            deleted[best] = 1;
        } else {
            deleted[worst] = 1;
        }
    }
}

void doCrossover(Individual island[], int deleted[]) {
    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if (deleted[i]) {
            int p1 = rand() % ISLAND_POP_SIZE;
            while (deleted[p1]) {
                p1 = rand() % ISLAND_POP_SIZE;
            }

            int p2 = rand() % ISLAND_POP_SIZE;
            while (p1 == p2 || deleted[p2]) {
                p2 = rand() % ISLAND_POP_SIZE;
            }

            for (int j = 0; j < 81; j++) {
                if (rand() % 2 == 0) {
                    island[i].cromossome[j] = island[p1].cromossome[j];
                } else {
                    island[i].cromossome[j] = island[p2].cromossome[j];
                }
            }
        }
    }
}

void doCrossoverNoLines(Individual island[], int deleted[]) {
    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if (deleted[i]) {
            int p1 = rand() % ISLAND_POP_SIZE;
            while (deleted[p1]) {
                p1 = rand() % ISLAND_POP_SIZE;
            }

            int p2 = rand() % ISLAND_POP_SIZE;
            while (p1 == p2 || deleted[p2]) {
                p2 = rand() % ISLAND_POP_SIZE;
            }

            for (int j = 0; j < 9; j++) {
                if (rand() % 2 == 0) {
                    for (int k = 0; k < 9; k++) {
                        island[i].cromossome[j*9 + k] = island[p1].cromossome[j*9 + k];
                    }
                } else {
                    for (int k = 0; k < 9; k++) {
                        island[i].cromossome[j*9 + k] = island[p2].cromossome[j*9 + k];
                    }
                }
            }
        }
    }
}

void doMutation(Individual island[], int cluesMask[], int deleted[], int noImprovement) {
    float mutationChance = MUTATION_RATE + (MUTATION_GROWTH * noImprovement);
    if (mutationChance > MUTATION_THRESHOLD) mutationChance = MUTATION_THRESHOLD;

    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if(deleted[i]) {
            for (int j = 0; j < 81; j++) {
                if (cluesMask[j] == 0 && ((float)rand() / RAND_MAX) < mutationChance) {
                    island[i].cromossome[j] = (rand() % 9) + 1;
                }
            }
        }
    }
}

void doMutationNoLines(Individual island[], int cluesMask[], int deleted[], int noImprovement) {
    float mutationChance = MUTATION_RATE + (MUTATION_GROWTH * noImprovement);
    if (mutationChance > MUTATION_THRESHOLD) mutationChance = MUTATION_THRESHOLD;

    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if(deleted[i]) {
            for (int j = 0; j < 9; j++) {
                if (((float)rand() / RAND_MAX) < mutationChance) {

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

void doMutationV2(Individual island[], int cluesMask[], int deleted[], int noImprovement) {
    float mutationChance = MUTATION_RATE + (MUTATION_GROWTH * noImprovement);
    if (mutationChance > MUTATION_THRESHOLD) mutationChance = MUTATION_THRESHOLD;

    for (int i = 0; i < ISLAND_POP_SIZE; i++) {
        if(deleted[i]) {
            for (int row = 0; row < 9; row++) {
                
                if (((float)rand() / RAND_MAX) < mutationChance) {
                    
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
                                        if (actualRow != row && actualCol != col && 
                                            island[i].cromossome[actualRow * 9 + actualCol] == val) {
                                            hasError = 1; r = 3; break;
                                        }
                                    }
                                }
                            }

                            if (hasError) {
                                errorCells[errorCount++] = col;
                            } else {
                                perfectCells[perfectCount++] = col;
                            }
                        }
                    }

                    if (errorCount > 0) {
                        int c1 = errorCells[rand() % errorCount];
                        int c2;

                        if (errorCount > 1 && ((float)rand() / RAND_MAX) < 0.70) {
                            c2 = errorCells[rand() % errorCount];
                            while(c1 == c2) {
                                c2 = errorCells[rand() % errorCount];
                            }
                        } 
                        else if (perfectCount > 0) {
                            c2 = perfectCells[rand() % perfectCount];
                        } 
                        else {
                            continue;
                        }

                        int tmp = island[i].cromossome[row * 9 + c1];
                        island[i].cromossome[row * 9 + c1] = island[i].cromossome[row * 9 + c2];
                        island[i].cromossome[row * 9 + c2] = tmp;

                    } 
                    else if (perfectCount >= 2 && noImprovement > 200) {
                        int c1 = perfectCells[rand() % perfectCount];
                        int c2 = perfectCells[rand() % perfectCount];
                        while(c1 == c2) c2 = perfectCells[rand() % perfectCount];
                        
                        int tmp = island[i].cromossome[row * 9 + c1];
                        island[i].cromossome[row * 9 + c1] = island[i].cromossome[row * 9 + c2];
                        island[i].cromossome[row * 9 + c2] = tmp;
                    }
                }
            }
        }
    }
}

void doMigration(Individual (*population)[ISLAND_POP_SIZE]) {
    Individual bestIndividuals[ISLANDS][MIGRATION_SIZE];
    int worstIndividuals[ISLANDS][MIGRATION_SIZE];

    for (int i = 0; i < ISLANDS; i++) {
        for (int m = 0; m < MIGRATION_SIZE; m++) {
            
            int fighterA = rand() % ISLAND_POP_SIZE;
            int fighterB = rand() % ISLAND_POP_SIZE;
            int fighterC = rand() % ISLAND_POP_SIZE;

            int bIdx = fighterA;
            if (population[i][fighterB].fitness < population[i][bIdx].fitness) {
                bIdx = fighterB;
            }
            if (population[i][fighterC].fitness < population[i][bIdx].fitness) {
                bIdx = fighterC;
            }

            bestIndividuals[i][m] = population[i][bIdx];

            int loserA = rand() % ISLAND_POP_SIZE;
            int loserB = rand() % ISLAND_POP_SIZE;
            int loserC = rand() % ISLAND_POP_SIZE;

            int wIdx = loserA;
            if (population[i][loserB].fitness > population[i][wIdx].fitness) {
                wIdx = loserB;
            }
            if (population[i][loserC].fitness > population[i][wIdx].fitness) {
                wIdx = loserC;
            }

            worstIndividuals[i][m] = wIdx;
        }
    }

    // Efetiva a transferencia em anel para a proxima ilha
    for (int i = 0; i < ISLANDS; i++) {
        int nextIsland = (i + 1) % ISLANDS;
        for (int m = 0; m < MIGRATION_SIZE; m++) {
            population[nextIsland][worstIndividuals[nextIsland][m]] = bestIndividuals[i][m];
        }
    }
}

void geneticAlgorithm(int tabuleiro[81], int cluesMask[81], int silentMode, int problemLine, int runId) {
    Individual (*population)[ISLAND_POP_SIZE] = malloc(ISLANDS * sizeof(*population));

    if (population == NULL) {
        printf("Falha ao alocar populacao\n");
        exit(1);
    }

    int globalBest = 999;
    int noImprovement = 0;

    char filename[128];
    
    sprintf(filename, "Graphs/problem_%d_run_%d/fitness_log_linha_%d.csv", problemLine, runId, problemLine);
    
    FILE *logFile = fopen(filename, "w");
    if (logFile) {
        fprintf(logFile, "Geracao,MelhorGlobal,MelhorGeracao\n");
    }

    Individual bestSolution;
    bestSolution.fitness = 999;

    int islandsBest[ISLANDS];
    for (int i = 0; i < ISLANDS; i++) {
        islandsBest[i] = 999;
    }
    
    int noImprovementIsland[ISLANDS] = {0};

    //Gera populacao inicial aleatoria
    /*
    for (int i = 0; i < ISLANDS; i++) {
        //printf("Ilha %d\n", i);
        //printf("\n");

        for (int j = 0; j < ISLAND_POP_SIZE; j++) {
            population[i][j].fitness = 0.0f;
            
            for (int k = 0; k < 81; k++) {
                if (cluesMask[k] == 1) {
                    population[i][j].cromossome[k] = tabuleiro[k];
                } else {
                    population[i][j].cromossome[k] = rand() % 9 + 1;
                }
            }
        }
    }
    */

    //Gera populacao inicial aleatoria, porem sem repetir numeros nas linhas
    for (int i = 0; i < ISLANDS; i++) {
        for (int j = 0; j < ISLAND_POP_SIZE; j++) {
            population[i][j].fitness = 0;
                        for (int row = 0; row < 9; row++) {
                int available[9];       
                int availableCount = 0;
                int used[10] = {0};
                for (int col = 0; col < 9; col++) {
                    int index = row * 9 + col;
                    if (cluesMask[index] == 1) {
                        population[i][j].cromossome[index] = tabuleiro[index];
                        used[tabuleiro[index]] = 1;
                    }
                }
                for (int num = 1; num <= 9; num++) {
                    if (!used[num]) {
                        available[availableCount++] = num;
                    }
                }
                for (int k = availableCount - 1; k > 0; k--) {
                    int r = rand() % (k + 1);
                    int temp = available[k];
                    available[k] = available[r];
                    available[r] = temp;
                }
                int fillIdx = 0;
                for (int col = 0; col < 9; col++) {
                    int index = row * 9 + col;
                    if (cluesMask[index] == 0) {
                        population[i][j].cromossome[index] = available[fillIdx++];
                    }
                }
            }
        }
    }

    clock_t start_time = clock();

    int lastGen = 0;

    for (int generation = 0; generation < MAX_GENERATIONS; generation++) {
        lastGen++;
        int generationalBest = 999;
        int hasElite[ISLANDS] = {0};
        Individual islandElites[ISLANDS];

        //Calcula o fitness da populacao de cada ilha
        for (int i = 0; i < ISLANDS; i++) {
            int bestIdx = calculateFitnessNoLines(population[i]);
            int bestFitness = population[i][bestIdx].fitness;

            // Atualiza os recordes globais e geracionais
            if (bestFitness < generationalBest) generationalBest = bestFitness;

            if (bestFitness < bestSolution.fitness) {
                bestSolution = population[i][bestIdx];
            }
            
            //Verifica se ha um individuo na ilha que eh melhor que todos os outros, se sim, protege ele
            int bestCount = 0;
            for (int j = 0; j < ISLAND_POP_SIZE; j++) {
                if (population[i][j].fitness == bestFitness) {
                    bestCount++;
                }
            }

            if (bestCount == 1) {
                hasElite[i] = 1;
                islandElites[i] = population[i][bestIdx];
            } else {
                hasElite[i] = 0;
            }
            
            if (bestFitness < islandsBest[i]) {
                islandsBest[i] = bestFitness;
                noImprovementIsland[i] = 0;
            } else {
                noImprovementIsland[i]++;
            }
        }

        //Imprime ha quantas geracoes nao ha melhora global
        if ((generation % MIGRATION_FREQUENCY == 0) || generation == MAX_GENERATIONS - 1) {
            
            if (generationalBest < globalBest) {
                globalBest = generationalBest;
                noImprovement = 0;
            } else {
                noImprovement += MIGRATION_FREQUENCY;
            }

            if (logFile) {
                fprintf(logFile, "%d,%d,%d\n", generation, globalBest, generationalBest);
            }

            // Solucao encontrada
            if (globalBest == 0) {
                break;
            }
        }

        if (!silentMode && generation % 10 == 0) {
            printReport(population, generation, noImprovementIsland, globalBest);
        }

        //Realiza migracao apos X geracoes
        if (generation > 0 && generation % MIGRATION_FREQUENCY == 0) {
            doMigration(population);
        }
        
        for (int i = 0; i < ISLANDS; i++) {
            int deleted[ISLAND_POP_SIZE] = {0};

            int eliteFit = hasElite[i] ? islandElites[i].fitness : -1;

            doTournamentV2(population[i], deleted, eliteFit);
            //printDeleted(deleted);

            doCrossoverNoLines(population[i], deleted);

            doMutationV2(population[i], cluesMask, deleted, noImprovementIsland[i]);
        }

        //printPopulation(population);
    }
    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    int hours = (int)(time_spent / 3600);
    int minutes = ((int)time_spent % 3600) / 60;
    double seconds = time_spent - (hours * 3600) - (minutes * 60);

    if (logFile) fclose(logFile);

    char reportName[128];
    sprintf(reportName, "Graphs/problem_%d_run_%d/relatorio_execucao.txt", problemLine, runId);
    FILE *reportFile = fopen(reportName, "w");
    
    if (reportFile) {
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, " RELATORIO TÉCNICO - SUDOKU LINHA %d\n", problemLine);
        fprintf(reportFile, "==================================================\n");
        
        fprintf(reportFile, " HIPERPARAMETROS:\n");
        fprintf(reportFile, " Pop. Total: %d | Ilhas: %d | Max Gen: %d\n", POP_SIZE, ISLANDS, MAX_GENERATIONS);
        fprintf(reportFile, " Mutacao Base: %.3f | Freq. Migracao: %d\n", MUTATION_RATE, MIGRATION_FREQUENCY);
        fprintf(reportFile, "==================================================\n");
        
        if (globalBest == 0) {
            fprintf(reportFile, "STATUS: RESOLVIDO COM SUCESSO!\n");
        } else {
            fprintf(reportFile, "STATUS: LIMITE DE GERACOES ATINGIDO (ESTAGNADO)\n");
        }
        
        fprintf(reportFile, "Fitness Final (Erros): %d\n", globalBest);
        fprintf(reportFile, "Geracoes Executadas: %d / %d\n", lastGen, MAX_GENERATIONS);
        fprintf(reportFile, "Tempo Total de CPU: %.3f segundos\n", time_spent);
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, "TABULEIRO FINAL DA MELHOR SOLUCAO:\n");
        
        for (int i = 0; i < 81; i++) {
            fprintf(reportFile, "%d ", bestSolution.cromossome[i]);
            if ((i + 1) % 9 == 0) fprintf(reportFile, "\n");
        }
        fclose(reportFile);
    }

    if(!silentMode) {
        printf("Melhor fitness encontrado: %d.\n", globalBest);
        printf("Nenhuma solucao encontrada apos %d geracoes.\n", MAX_GENERATIONS);
        printf("Tempo total de processamento: %.2f segundos\n", time_spent);
        printf("Tempo total de processamento: %02d horas, %02d minutos e %.2f segundos\n", hours, minutes, seconds);

        printf("\n==================================================\n");
        printf("MELHOR SOLUCAO ENCONTRADA (%d erros):\n", bestSolution.fitness);
        printHighlightedSolution(bestSolution);
        printf("==================================================\n");
    }

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

    if (argc > 1) {
        problemLine = atoi(argv[1]);
    } else {
        problemLine = (rand() % 3000000) + 1;
    }

    if (argc > 2) {
        silentMode = atoi(argv[2]);
    }

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

            //printSudoku(sudoku);

            int cluesMask[81] = {0};
            for (int i = 0; i < 81; i++) {
                if (sudoku.puzzle[i] != 0) {
                    cluesMask[i] = 1;
                }
            }

            //printCluesMask(cluesMask);

            printf("Iniciando algoritmo genetico\n");
            srand(GLOBAL_SEED + problemLine + runId);

            geneticAlgorithm(sudoku.puzzle, cluesMask, silentMode, problemLine, runId);
        }
    }

    fclose(file);
    return 0;
}