#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h> // Para isprint()

#define MAX_TREE_NODES 256
#define MAX_CODE_LENGTH 256
#define MAX_FILES 100
#define FOLDER_NAME "books"

typedef struct HuffmanNode {
    unsigned char symbol;
    int frequency;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct PriorityQueue {
    HuffmanNode *nodes[MAX_TREE_NODES];
    int size;
} PriorityQueue;

HuffmanNode *createNode(unsigned char symbol, int frequency) {
    HuffmanNode *node = (HuffmanNode *)malloc(sizeof(HuffmanNode));
    if (!node) {
        perror("Error creando nodo");
        exit(EXIT_FAILURE);
    }
    node->symbol = symbol;
    node->frequency = frequency;
    node->left = node->right = NULL;
    return node;
}

void initPriorityQueue(PriorityQueue *pq) {
    pq->size = 0;
}

void enqueue(PriorityQueue *pq, HuffmanNode *node) {
    int i = pq->size;
    while (i > 0 && pq->nodes[(i - 1) / 2]->frequency > node->frequency) {
        pq->nodes[i] = pq->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    pq->nodes[i] = node;
    pq->size++;
}

HuffmanNode *dequeue(PriorityQueue *pq) {
    HuffmanNode *minNode = pq->nodes[0];
    pq->size--;
    HuffmanNode *lastNode = pq->nodes[pq->size];
    int i = 0;
    while (2 * i + 1 < pq->size) {
        int child = 2 * i + 1;
        if (child + 1 < pq->size && pq->nodes[child + 1]->frequency < pq->nodes[child]->frequency) {
            child++;
        }
        if (lastNode->frequency <= pq->nodes[child]->frequency) {
            break;
        }
        pq->nodes[i] = pq->nodes[child];
        i = child;
    }
    pq->nodes[i] = lastNode;
    return minNode;
}

HuffmanNode *buildHuffmanTree(int *frequencies) {
    PriorityQueue pq;
    initPriorityQueue(&pq);

    int symbol_count = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            enqueue(&pq, createNode((unsigned char)i, frequencies[i]));
            symbol_count++;
        }
    }

    // Caso especial: solo un símbolo
    if (symbol_count == 1) {
        HuffmanNode *only = dequeue(&pq);
        HuffmanNode *root = createNode(0, only->frequency);
        root->left = only;
        return root;
    }

    while (pq.size > 1) {
        HuffmanNode *left = dequeue(&pq);
        HuffmanNode *right = dequeue(&pq);
        HuffmanNode *parent = createNode(0, left->frequency + right->frequency);
        parent->left = left;
        parent->right = right;
        enqueue(&pq, parent);
    }

    return pq.size > 0 ? dequeue(&pq) : NULL;
}

void generateCodes(HuffmanNode *root, char **codes, char *currentCode, int depth) {
    if (!root || depth >= MAX_CODE_LENGTH-1) return;

    if (!root->left && !root->right) {
        currentCode[depth] = '\0';
        codes[root->symbol] = strdup(currentCode);
        if (!codes[root->symbol]) {
            perror("Error asignando código");
            exit(EXIT_FAILURE);
        }
        return;
    }

    if (root->left) {
        currentCode[depth] = '0';
        generateCodes(root->left, codes, currentCode, depth+1);
    }
    
    if (root->right) {
        currentCode[depth] = '1';
        generateCodes(root->right, codes, currentCode, depth+1);
    }
}

void freeHuffmanTree(HuffmanNode *root) {
    if (!root) return;
    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}


void calculateFrequencies(FILE *input, int *frequencies) {
    unsigned char symbol;
    while (fread(&symbol, sizeof(unsigned char), 1, input) == 1) {
        frequencies[symbol]++;
    }
    rewind(input);
}

void saveCodesToFile(char **codes, FILE *output) {
    for (int i = 0; i < 256; i++) {
        if (codes[i] != NULL) {
            fprintf(output, "%d:%s\n", i, codes[i]);
        }
    }
}

void validateCodes(char **codes, int *frequencies) {
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0 && codes[i] == NULL) {
            fprintf(stderr, "Error crítico: Símbolo %d ('%c') tiene frecuencia %d pero no tiene código asignado\n",
                   i, isprint(i) ? i : '.', frequencies[i]);
            exit(EXIT_FAILURE);
        }
    }
}

void compressMultipleFiles(FILE *output, char **codes, char **filenames, int fileCount) {
    unsigned char symbol;
    for (int i = 0; i < fileCount; i++) {
        FILE *input = fopen(filenames[i], "rb");
        if (!input) {
            fprintf(stderr, "Error al abrir %s\n", filenames[i]);
            continue;
        }

        fputs("FILE_SEP", output);

        while (fread(&symbol, sizeof(unsigned char), 1, input) == 1) {
            if (codes[symbol]) {
                fputs(codes[symbol], output);
            } else {
                fprintf(stderr, "Error: Símbolo %d no tiene código (archivo: %s)\n", symbol, filenames[i]);
                exit(EXIT_FAILURE);
            }
        }
        fclose(input);
    }
}

int main() {
    DIR *dir;
    struct dirent *ent;
    char *filenames[MAX_FILES] = {NULL};
    int fileCount = 0;
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "./%s", FOLDER_NAME);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Carpeta '%s' no encontrada\n", FOLDER_NAME);
        return 1;
    }

    if (!(dir = opendir(path))) {
        perror("Error abriendo directorio");
        return 1;
    }

    while ((ent = readdir(dir)) && fileCount < MAX_FILES) {
        if (strstr(ent->d_name, ".txt")) {
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
            
            FILE *test = fopen(fullpath, "rb");
            if (test) {
                fclose(test);
                filenames[fileCount] = strdup(fullpath);
                if (!filenames[fileCount]) {
                    perror("Error asignando memoria");
                    closedir(dir);
                    return 1;
                }
                fileCount++;
            }
        }
    }
    closedir(dir);

    if (fileCount == 0) {
        printf("No se encontraron archivos .txt en '%s'\n", FOLDER_NAME);
        return 0;
    }

    printf("Archivos a comprimir:\n");
    for (int i = 0; i < fileCount; i++) {
        printf("- %s\n", filenames[i]);
    }

    int frequencies[256] = {0};
    for (int i = 0; i < fileCount; i++) {
        FILE *input = fopen(filenames[i], "rb");
        if (input) {
            calculateFrequencies(input, frequencies);
            fclose(input);
        }
    }

    // Verificar que hayamos leído caracteres
    int total_chars = 0;
    for (int i = 0; i < 256; i++) total_chars += frequencies[i];
    if (total_chars == 0) {
        fprintf(stderr, "Error: No se encontraron caracteres en los archivos de entrada\n");
        return 1;
    }

    HuffmanNode *root = buildHuffmanTree(frequencies);
    if (!root) {
        fprintf(stderr, "Error: No se pudo construir el árbol de Huffman\n");
        return 1;
    }

    char *codes[256] = {NULL};
    char currentCode[MAX_CODE_LENGTH] = {0};
    generateCodes(root, codes, currentCode, 0);

    // Validación crítica
    validateCodes(codes, frequencies);

    FILE *codesFile = fopen("codes.txt", "w");
    if (codesFile) {
        saveCodesToFile(codes, codesFile);
        fclose(codesFile);
    }

    FILE *outputFile = fopen("textoComprimido.txt", "w");
    if (outputFile) {
        compressMultipleFiles(outputFile, codes, filenames, fileCount);
        fclose(outputFile);
    }

    printf("\nCompresión completada:\n");
    printf("- %d archivos comprimidos\n", fileCount);

    // Liberar memoria
    freeHuffmanTree(root);
    for (int i = 0; i < 256; i++) {
        free(codes[i]);
    }
    for (int i = 0; i < fileCount; i++) {
        free(filenames[i]);  // Solo liberar una vez
    }

    return 0;
}