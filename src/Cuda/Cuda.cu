#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cuda_runtime.h>
#include <curand_kernel.h> 
#include "../../config.h"
/*

#define MAX_GENERATIONS 2000
#define POP_SIZE 196608
#define ISLANDS 384
#define ISLAND_POP_SIZE (POP_SIZE / ISLANDS)
#define WARPS_PER_ISLAND (ISLAND_POP_SIZE / 32)

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

//Funcao de reducao para os warps
__device__ int warpReduceMin(int val) {
    for (int offset = WARPS_PER_ISLAND; offset > 0; offset /= 2) {
        int shfl = __shfl_down_sync(0xffffffff, val, offset);
        if (shfl < val) {
            val = shfl;
        }
    }

    return val;
}

//Funcao de Crossover adaptada
__device__ void doCrossoverCuda(int* d_chromosomes, int globalId, int islandId, int* deleted, curandState* localState) {
    int p1 = curand(localState) % ISLAND_POP_SIZE;
    while (deleted[p1]) {
        p1 = curand(localState) % ISLAND_POP_SIZE;
    }

    int p2 = curand(localState) % ISLAND_POP_SIZE;
    while (p1 == p2 || deleted[p2]) {
        p2 = curand(localState) % ISLAND_POP_SIZE;
    }

    int globalP1 = islandId * ISLAND_POP_SIZE + p1;
    int globalP2 = islandId * ISLAND_POP_SIZE + p2;

    for (int row = 0; row < 9; row++) {
        int parentSource = (curand(localState) % 2 == 0) ? globalP1 : globalP2;
        for (int col = 0; col < 9; col++) {
            int geneIdx = row * 9 + col;
            d_chromosomes[geneIdx * POP_SIZE + globalId] = d_chromosomes[geneIdx * POP_SIZE + parentSource];
        }
    }
}
/*  Funcao de mutacao com heuristica adaptada
*   instrucoes utilizadas: 
*   __ldg(): Le uma variavel diretamente do cache de leitura
*   __popc(): Conta quantos bits 1 existem na variavel
*   __ffs(): Encontra a posicao do primeiro bit 1
*/
__device__ void doMutationCuda(int* d_chromosomes, int globalId, int islandId, int* cluesMask, int noImprovement, curandState* localState) {                     
    float mutationChance = fminf(MUTATION_RATE + (MUTATION_GROWTH * noImprovement), MUTATION_THRESHOLD);

    for (int row = 0; row < 9; row++) {
        if (curand_uniform(localState) < mutationChance) {
            
            unsigned int errorMask = 0;
            unsigned int perfectMask = 0;

            for(int col = 0; col < 9; col++) {
                int geneIdx = row * 9 + col;
                
                if(__ldg(&cluesMask[geneIdx]) == 0) {
                    int val = d_chromosomes[geneIdx * POP_SIZE + globalId];
                    int hasError = 0;

                    for (int r = 0; r < 9; r++) {
                        if (r != row && d_chromosomes[(r * 9 + col) * POP_SIZE + globalId] == val) {
                            hasError = 1;
                            break; 
                        }
                    }

                    if (!hasError) {
                        int startRow = (row / 3) * 3;
                        int startCol = (col / 3) * 3;
                        
                        #pragma unroll
                        for (int i = 0; i < 9; i++) {
                            int r = i / 3;
                            int c = i % 3;
                            int actualRow = startRow + r;
                            int actualCol = startCol + c;
                            
                            if (actualRow != row && actualCol != col && 
                                d_chromosomes[(actualRow * 9 + actualCol) * POP_SIZE + globalId] == val) {
                                hasError = 1; 
                                break; 
                            }
                        }
                    }

                    if (hasError) {
                        errorMask |= (1 << col);
                    } else {
                        perfectMask |= (1 << col);
                    }
                }
            }

            int errorCount = __popc(errorMask);
            int perfectCount = __popc(perfectMask);

            if (errorCount > 0) {
                int randIdx = curand(localState) % errorCount;
                int c1 = -1;
                unsigned int tempMask = errorMask;
                
                for (int i = 0; i <= randIdx; i++) {
                    c1 = __ffs(tempMask) - 1; 
                    tempMask &= ~(1 << c1);
                }

                int c2 = c1;

                if (errorCount > 1 && curand_uniform(localState) < 0.70f) {
                    int randIdx2 = curand(localState) % errorCount;
                    while(randIdx == randIdx2) {
                        randIdx2 = curand(localState) % errorCount; 
                    }
                    
                    tempMask = errorMask;
                    for (int i = 0; i <= randIdx2; i++) {
                        c2 = __ffs(tempMask) - 1;
                        tempMask &= ~(1 << c2);
                    }
                } 
                else if (perfectCount > 0) {
                    int randIdx2 = curand(localState) % perfectCount;
                    tempMask = perfectMask;
                    for (int i = 0; i <= randIdx2; i++) {
                        c2 = __ffs(tempMask) - 1;
                        tempMask &= ~(1 << c2);
                    }
                } 

                if (c1 != c2) {
                    int geneIdx1 = row * 9 + c1;
                    int geneIdx2 = row * 9 + c2;

                    int tmp = d_chromosomes[geneIdx1 * POP_SIZE + globalId];

                    d_chromosomes[geneIdx1 * POP_SIZE + globalId] = d_chromosomes[geneIdx2 * POP_SIZE + globalId];
                    d_chromosomes[geneIdx2 * POP_SIZE + globalId] = tmp;
                }
            } 
            else if (perfectCount >= 2 && noImprovement > 200) {
                int randIdx1 = curand(localState) % perfectCount;
                int randIdx2 = curand(localState) % perfectCount;

                while(randIdx1 == randIdx2) {
                    randIdx2 = curand(localState) % perfectCount;
                }

                int c1 = -1, c2 = -1;
                unsigned int tempMask = perfectMask;

                for (int i = 0; i <= randIdx1; i++) {
                    c1 = __ffs(tempMask) - 1;
                    tempMask &= ~(1 << c1);
                }
                
                tempMask = perfectMask;
                for (int i = 0; i <= randIdx2; i++) {
                    c2 = __ffs(tempMask) - 1;
                    tempMask &= ~(1 << c2);
                }
                
                int geneIdx1 = row * 9 + c1;
                int geneIdx2 = row * 9 + c2;

                int tmp = d_chromosomes[geneIdx1 * POP_SIZE + globalId];

                d_chromosomes[geneIdx1 * POP_SIZE + globalId] = d_chromosomes[geneIdx2 * POP_SIZE + globalId];
                d_chromosomes[geneIdx2 * POP_SIZE + globalId] = tmp;
            }
        }
    }
}

//Funcao de migracao assincrona adaptada
__device__ void doMigrationCuda(int* d_chromosomes, int* d_fitness, int islandId, int globalId, int generation, curandState* state, int* d_mailbox, int* mailboxFull) {
    if (atomicCAS(&mailboxFull[islandId], 1, 2) == 1) {

        for (int m = 0; m < MIGRATION_SIZE; m++) {
            int loserA = curand(&state[globalId]) % ISLAND_POP_SIZE;
            int loserB = curand(&state[globalId]) % ISLAND_POP_SIZE;
            int loserC = curand(&state[globalId]) % ISLAND_POP_SIZE;

            int fitLoserA = d_fitness[islandId * ISLAND_POP_SIZE + loserA];
            int fitLoserB = d_fitness[islandId * ISLAND_POP_SIZE + loserB];
            int fitLoserC = d_fitness[islandId * ISLAND_POP_SIZE + loserC];

            int worstIdx = loserA;
            int maxFit = fitLoserA;

            if (fitLoserB > maxFit) { 
                worstIdx = loserB; 
                maxFit = fitLoserB; 
            }
            if (fitLoserC > maxFit) { 
                worstIdx = loserC; 
                maxFit = fitLoserC; 
            }

            int mailboxOffset = (islandId * MIGRATION_SIZE * 81) + (m * 81);
            int indGlobalIdx = islandId * ISLAND_POP_SIZE + worstIdx;
            
            for(int i = 0; i < 81; i++) {
                d_chromosomes[i * POP_SIZE + indGlobalIdx] = d_mailbox[mailboxOffset + i];
            }
        }
        atomicExch(&mailboxFull[islandId], 0); 
    }

    if (generation > 0 && generation % MIGRATION_FREQUENCY == 0) {
        int nextIsland = (islandId + 1) % ISLANDS;
        
        if (atomicCAS(&mailboxFull[nextIsland], 0, 2) == 0) {
            for (int m = 0; m < MIGRATION_SIZE; m++) {
                int fighterA = curand(&state[globalId]) % ISLAND_POP_SIZE;
                int fighterB = curand(&state[globalId]) % ISLAND_POP_SIZE;
                int fighterC = curand(&state[globalId]) % ISLAND_POP_SIZE;

                int fitFighterA = d_fitness[islandId * ISLAND_POP_SIZE + fighterA];
                int fitFighterB = d_fitness[islandId * ISLAND_POP_SIZE + fighterB];
                int fitFighterC = d_fitness[islandId * ISLAND_POP_SIZE + fighterC];

                int bestIdx = fighterA; 
                int minFit = fitFighterA;
                
                if (fitFighterB < minFit) { 
                    bestIdx = fighterB; 
                    minFit = fitFighterB; 
                }
                if (fitFighterC < minFit) { 
                    bestIdx = fighterC; 
                    minFit = fitFighterC; 
                }

                int mailboxOffset = (nextIsland * MIGRATION_SIZE * 81) + (m * 81);
                int indGlobalIdx = islandId * ISLAND_POP_SIZE + bestIdx;
                
                for(int i = 0; i < 81; i++) {
                    d_mailbox[mailboxOffset + i] = d_chromosomes[i * POP_SIZE + indGlobalIdx];
                }
            }
            atomicExch(&mailboxFull[nextIsland], 1); 
        }
    }
}

//Funcao de torneio adaptada e paralelizada
__device__ void doTournamentCuda(int* d_fitness, int islandId, int globalId, int* deleted, curandState* state, int eliteFit) {
    bool killConfirmed = false;

    while (!killConfirmed) {
        int f1 = curand(state) % ISLAND_POP_SIZE;
        int f2 = curand(state) % ISLAND_POP_SIZE;

        if (f1 == f2 || deleted[f1] || deleted[f2]) {
            continue;
        }

        int fit1 = d_fitness[islandId * ISLAND_POP_SIZE + f1];
        int fit2 = d_fitness[islandId * ISLAND_POP_SIZE + f2];

        int best = (fit1 < fit2) ? f1 : f2;
        int worst = (f1 == best) ? f2 : f1;
        int diff = abs(fit2 - fit1); 
        
        float upsetChance = 0.01f;
        
        upsetChance = (diff <= 8) ? 0.10f : upsetChance;
        upsetChance = (diff <= 4) ? 0.25f : upsetChance;
        upsetChance = (diff <= 2) ? 0.50f : upsetChance;
        upsetChance = (fit1 <= eliteFit || diff == 0) ? 0.0f : upsetChance;

        int target = (curand_uniform(state) < upsetChance) ? best : worst;

        killConfirmed = (atomicCAS(&deleted[target], 0, 1) == 0);
    }
}

__global__ void setupRandomKernel(curandState* state, unsigned long seed) {
    int id = threadIdx.x + blockIdx.x * blockDim.x;
    curand_init(seed, id, 0, &state[id]);
}

__global__ void geneticKernel(int* d_chromosomes, int* d_fitness, int* cluesMask, int* noImprovementIsland, int* bestHistoryIsland, curandState* state, int generation, int* d_mailbox, int* mailboxFull, int* d_generationalBest, int* d_stopFlag) {
                                       
    int islandId = blockIdx.x;  
    int threadId = threadIdx.x; 
    int globalId = threadId + blockIdx.x * blockDim.x;

    int mistakes = 0;
    
    //Calculo de fitness
    #pragma unroll
    for(int sector = 0; sector < 9; sector++) {
        unsigned int colMask = 0;
        unsigned int boxMask = 0;

        int boxRow = (sector / 3) * 3;
        int boxCol = (sector % 3) * 3;

        for (int idx = 0; idx < 9; idx++) {
            int valCol = d_chromosomes[(idx * 9 + sector) * POP_SIZE + globalId];
            int valBox = d_chromosomes[((boxRow + idx / 3) * 9 + (boxCol + idx % 3)) * POP_SIZE + globalId];
            
            if (colMask & (1 << valCol)) {
                mistakes++;
            } else {
                colMask |= (1 << valCol);
            }

            if (boxMask & (1 << valBox)) {
                mistakes++;
            } else {
                boxMask |= (1 << valBox);
            }
        }
    }

    d_fitness[globalId] = mistakes;

    //Warp shuffle para determinar melhor fitness da ilha
    int warpId = threadIdx.x / 32; 
    int laneId = threadIdx.x % 32; 

    int myBest = warpReduceMin(mistakes);

    __shared__ int warpMins[WARPS_PER_ISLAND];
    __shared__ int deleted[ISLAND_POP_SIZE];
    __shared__ int sharedEliteFit;
    
    deleted[threadId] = 0; 

    if (laneId == 0) warpMins[warpId] = myBest;
    
    __syncthreads(); 

    if (warpId == 0) {
        int val = (laneId < WARPS_PER_ISLAND) ? warpMins[laneId] : 999;
        int localBest = warpReduceMin(val);

        if (laneId == 0) {
            sharedEliteFit = localBest;

            atomicMin(d_generationalBest, localBest);
            if (localBest == 0) {
                atomicExch(d_stopFlag, 1);
            }
            if (localBest < bestHistoryIsland[islandId]) {
                bestHistoryIsland[islandId] = localBest;
                noImprovementIsland[islandId] = 0;
            } else {
                noImprovementIsland[islandId]++;
            }

            doMigrationCuda(d_chromosomes, d_fitness, islandId, globalId, generation, state, d_mailbox, mailboxFull);
        }
    }
    
    __syncthreads(); 

    if (*d_stopFlag == 1) return;

    if (threadId < TOURNAMENT_SIZE) {
        curandState* localState = &state[globalId];
        doTournamentCuda(d_fitness, islandId, globalId, deleted, localState, sharedEliteFit);
    }
    
    __syncthreads();

    if (deleted[threadId]) {
        curandState* localState = &state[globalId];
        doCrossoverCuda(d_chromosomes, globalId, islandId, deleted, localState);
        doMutationCuda(d_chromosomes, globalId, islandId, cluesMask, noImprovementIsland[islandId], localState);
    }
}

//Funcao da CPU. Inicia populacoes e aloca memorias de variaveis
void geneticAlgorithm(int tabuleiro[81], int cluesMask[81], int silentMode, int problemLine, int runId) {
    
    int *h_chromosomes = (int*)malloc(POP_SIZE * 81 * sizeof(int));
    int *h_fitness = (int*)malloc(POP_SIZE * sizeof(int));
    
    if (h_chromosomes == NULL || h_fitness == NULL) {
        printf("Erro ao alocar memoria para a populacao\n");
        exit(1);
    }

    for (int globalId = 0; globalId < POP_SIZE; globalId++) {
        h_fitness[globalId] = 0; 
        for (int row = 0; row < 9; row++) {
            int available[9];       
            int availableCount = 0;
            int used[10] = {0};
            
            for (int col = 0; col < 9; col++) {
                int index = row * 9 + col;
                if (cluesMask[index] == 1) {
                    h_chromosomes[index * POP_SIZE + globalId] = tabuleiro[index];
                    used[tabuleiro[index]] = 1;
                }
            }
            
            for (int num = 1; num <= 9; num++) {
                if (!used[num]) available[availableCount++] = num;
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
                    h_chromosomes[index * POP_SIZE + globalId] = available[fillIdx++];
                }
            }
        }
    }

    //Aloca memoria na gpu para cromossomo e fitness
    int *d_chromosomes, *d_fitness;
    cudaMalloc(&d_chromosomes, POP_SIZE * 81 * sizeof(int));
    cudaMalloc(&d_fitness, POP_SIZE * sizeof(int));
    
    cudaMemcpy(d_chromosomes, h_chromosomes, POP_SIZE * 81 * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_fitness, h_fitness, POP_SIZE * sizeof(int), cudaMemcpyHostToDevice);
    
    //Aloca memoria na gpu para mascara de dicas
    int *d_cluesMask;
    cudaMalloc(&d_cluesMask, 81 * sizeof(int));
    cudaMemcpy(d_cluesMask, cluesMask, 81 * sizeof(int), cudaMemcpyHostToDevice);

    int noImprovementIsland[ISLANDS] = {0};
    int bestHistoryIsland[ISLANDS];

    for(int i = 0; i < ISLANDS; i++) {
        bestHistoryIsland[i] = 999;
    }

    //Aloca memoria na gpu para variaveis de nao-melhora
    int *d_noImprovementIsland, *d_bestHistoryIsland;
    cudaMalloc(&d_noImprovementIsland, ISLANDS * sizeof(int));
    cudaMalloc(&d_bestHistoryIsland, ISLANDS * sizeof(int));
    cudaMemcpy(d_noImprovementIsland, noImprovementIsland, ISLANDS * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_bestHistoryIsland, bestHistoryIsland, ISLANDS * sizeof(int), cudaMemcpyHostToDevice);

    //Aloca memoria na gpu para os estados de aleatoriedade
    curandState *d_state;
    cudaMalloc(&d_state, POP_SIZE * sizeof(curandState));

    //Aloca memoria para as mailboxes
    int *d_mailbox, *d_mailboxFull;
    cudaMalloc(&d_mailbox, ISLANDS * MIGRATION_SIZE * 81 * sizeof(int));
    cudaMalloc(&d_mailboxFull, ISLANDS * sizeof(int));
    cudaMemset(d_mailboxFull, 0, ISLANDS * sizeof(int));

    //Aloca memoria para controladores
    int *d_generationalBest, *d_stopFlag;
    cudaMalloc(&d_generationalBest, sizeof(int));
    cudaMalloc(&d_stopFlag, sizeof(int));
    int zero = 0;
    cudaMemcpy(d_stopFlag, &zero, sizeof(int), cudaMemcpyHostToDevice);

    //Inicia os estados de aleatoriedade
    setupRandomKernel<<<ISLANDS, ISLAND_POP_SIZE>>>(d_state, time(NULL));
    cudaDeviceSynchronize();

    char filename[128];
    sprintf(filename, "Graphs/problem_%d_run_%d/fitness_log_linha_%d.csv", problemLine, runId, problemLine);
    FILE *logFile = fopen(filename, "w");
    if (logFile) fprintf(logFile, "Geracao,MelhorGlobal,MelhorGeracao\n");

    int globalBest = 999;
    int bestSolution[81] = {0};
    
    double start_time = clock();
    int lastGen = 0;

    for (int generation = 0; generation < MAX_GENERATIONS; generation++) {
        lastGen++;
        
        geneticKernel<<<ISLANDS, ISLAND_POP_SIZE>>>(d_chromosomes, d_fitness, d_cluesMask, d_noImprovementIsland, d_bestHistoryIsland, d_state, generation, d_mailbox, d_mailboxFull, d_generationalBest, d_stopFlag);
        
        if ((generation > 0 && generation % MIGRATION_FREQUENCY == 0) || generation == MAX_GENERATIONS - 1) {
            int h_generationalBest = 999;
            int h_stopFlag;

            cudaMemcpy(&h_generationalBest, d_generationalBest, sizeof(int), cudaMemcpyDeviceToHost);
            cudaMemcpy(&h_stopFlag, d_stopFlag, sizeof(int), cudaMemcpyDeviceToHost);

            if (h_generationalBest < globalBest) {
                globalBest = h_generationalBest;
            }

            if (logFile) fprintf(logFile, "%d,%d,%d\n", generation, globalBest, h_generationalBest);

            if (h_stopFlag == 1 || globalBest == 0) {
                cudaMemcpy(h_chromosomes, d_chromosomes, POP_SIZE * 81 * sizeof(int), cudaMemcpyDeviceToHost);
                cudaMemcpy(h_fitness, d_fitness, POP_SIZE * sizeof(int), cudaMemcpyDeviceToHost);
                
                for (int i = 0; i < POP_SIZE; i++) {
                    if (h_fitness[i] == 0) {
                        for (int j = 0; j < 81; j++) bestSolution[j] = h_chromosomes[j * POP_SIZE + i];
                        break;
                    }
                }
                break; 
            }

            h_generationalBest = 999;
            cudaMemcpy(d_generationalBest, &h_generationalBest, sizeof(int), cudaMemcpyHostToDevice);
        }
    }
    
    double time_spent = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    if (logFile) fclose(logFile);

    char reportName[128];
    sprintf(reportName, "Graphs/problem_%d_run_%d/relatorio_execucao.txt", problemLine, runId);
    FILE *reportFile = fopen(reportName, "w");
    if (reportFile) {
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, " RELATORIO TECNICO CUDA V6 (100%% Paralelo) - LINHA %d\n", problemLine);
        fprintf(reportFile, "==================================================\n");
        fprintf(reportFile, " Pop. Total: %d | Ilhas: %d | Max Gen: %d\n", POP_SIZE, ISLANDS, MAX_GENERATIONS);
        fprintf(reportFile, " Mutacao Base: %.3f | Freq. Migracao: %d\n", MUTATION_RATE, MIGRATION_FREQUENCY);
        fprintf(reportFile, "==================================================\n");
        
        if (globalBest == 0) fprintf(reportFile, "STATUS: RESOLVIDO COM SUCESSO!\n");
        else fprintf(reportFile, "STATUS: LIMITE DE GERACOES ATINGIDO (ESTAGNADO)\n");
        
        fprintf(reportFile, "Fitness Final (Erros): %d\n", globalBest);
        fprintf(reportFile, "Geracoes Executadas: %d / %d\n", lastGen, MAX_GENERATIONS);
        fprintf(reportFile, "Tempo Total de CPU: %.3f segundos\n", time_spent);
        fprintf(reportFile, "==================================================\n");
        for (int i = 0; i < 81; i++) {
            fprintf(reportFile, "%d ", bestSolution[i]);
            if ((i + 1) % 9 == 0) fprintf(reportFile, "\n");
        }
        fclose(reportFile);
    }

    if (!silentMode) printf("Melhor fitness: %d. Tempo: %.2f seg\n", globalBest, time_spent);

    cudaFree(d_chromosomes);
    cudaFree(d_fitness);
    cudaFree(d_cluesMask);
    cudaFree(d_noImprovementIsland);
    cudaFree(d_bestHistoryIsland);
    cudaFree(d_state);
    cudaFree(d_mailbox);
    cudaFree(d_mailboxFull);
    cudaFree(d_generationalBest);
    cudaFree(d_stopFlag);
    free(h_chromosomes);
    free(h_fitness);
}

void getTabuleiro(const char* str_tab, int tabuleiro[81]) {
    for (int i = 0; i < 81; i++) {
        if (str_tab[i] == '.') tabuleiro[i] = 0;
        else tabuleiro[i] = str_tab[i] - '0';
    }
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("../../data/sudoku-3m.csv", "r");
    
    if(!file) file = fopen("../data/sudoku-3m.csv", "r");
    if(!file) file = fopen("data/sudoku-3m.csv", "r");
    
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
        srand(time(NULL));
        problemLine = (rand() % 3000000) + 1;
    }

    if (argc > 2) silentMode = atoi(argv[2]);

    int runId = 1;
    if (argc > 3) {
        runId = atoi(argv[3]);
    }

    int currentLine = 0;
    srand(time(NULL));

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

            printf("Iniciando algoritmo genetico CUDA\n");
            geneticAlgorithm(sudoku.puzzle, cluesMask, silentMode, problemLine, runId);
        }
    }

    fclose(file);
    return 0;
}