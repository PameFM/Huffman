#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#define MAX_TREE_NODES 256
#define MAX_CODE_LENGTH 256
#define MAX_FILES 100
#define FOLDER_NAME "books"
#define DELIMITER "FILE_SEP"

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

// Huffman tree functions
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

    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            enqueue(&pq, createNode((unsigned char)i, frequencies[i]));
        }
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

void saveCodesToFile(char **codes, FILE *output) {
    for (int i = 0; i < 256; i++) {
        if (codes[i] != NULL) {
            fprintf(output, "%d:%s\n", i, codes[i]);
        }
    }
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

// Codes loading
void loadCodesFromFile(const char *filename, char **codes) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error abriendo codes.txt");
        exit(EXIT_FAILURE);
    }
    int symbol;
    char code[MAX_CODE_LENGTH];
    while (fscanf(fp, "%d:%s\n", &symbol, code) == 2) {
        codes[symbol] = strdup(code);
    }
    fclose(fp);
}

// Compression
void writeBits(FILE *output, unsigned char *buffer, int *bitPos, const char *code) {
    for (int i = 0; code[i]; i++) {
        if (code[i] == '1') {
            *buffer |= (1 << (7 - *bitPos));
        }
        (*bitPos)++;
        if (*bitPos == 8) {
            fwrite(buffer, 1, 1, output);
            *buffer = 0;
            *bitPos = 0;
        }
    }
}
void compressFileChild(const char *inputFilename, const char *outputFilename) {
    char *codes[256] = {NULL};
    loadCodesFromFile("codes.txt", codes);

    FILE *input = fopen(inputFilename, "rb");
    FILE *output = fopen(outputFilename, "wb");
    if (!input || !output) {
        perror("Error abriendo archivos");
        exit(EXIT_FAILURE);
    }

    unsigned char symbol;
    unsigned char buffer = 0;
    int bitPos = 0;

    // Escribir FILE_SEP antes del contenido
    for (int j = 0; DELIMITER[j]; j++) {
        if (codes[(unsigned char)DELIMITER[j]]) {
            writeBits(output, &buffer, &bitPos, codes[(unsigned char)DELIMITER[j]]);
        } else {
            fprintf(stderr, "[ERROR] No hay código para el caracter de delimitador '%c'\n", DELIMITER[j]);
            fclose(input);
            fclose(output);
            exit(EXIT_FAILURE);
        }
    }

    // Comprimir el contenido normal
    while (fread(&symbol, 1, 1, input) == 1) {
        if (codes[symbol]) {
            writeBits(output, &buffer, &bitPos, codes[symbol]);
        } else {
            fprintf(stderr, "[ERROR] Símbolo %d (%c) sin código\n", symbol, isprint(symbol) ? symbol : '.');
            fclose(input);
            fclose(output);
            exit(EXIT_FAILURE);
        }
    }

    // Si sobran bits, escribir el último byte
    if (bitPos > 0) {
        fwrite(&buffer, 1, 1, output);
        buffer = 0;
        bitPos = 0;
    }

    fclose(input);
    fclose(output);

    for (int i = 0; i < 256; i++) free(codes[i]);
}


// Merge
void mergeTemporaryFiles(int fileCount) {
    FILE *finalOutput = fopen("textoComprimido.bin", "wb");
    if (!finalOutput) {
        perror("Error creando archivo final");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < fileCount; i++) {
        char tempFilename[64];
        snprintf(tempFilename, sizeof(tempFilename), "tmp_out_%d.bin", i);

        FILE *tempInput = fopen(tempFilename, "rb");
        if (!tempInput) {
            perror("Error abriendo archivo temporal");
            continue;
        }

        unsigned char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), tempInput)) > 0) {
            fwrite(buffer, 1, bytesRead, finalOutput);
        }

        fclose(tempInput);
        remove(tempFilename);
    }

    fclose(finalOutput);
}

// Main
int main() {
    DIR *dir;
    struct dirent *ent;
    char *filenames[MAX_FILES] = {NULL};
    int fileCount = 0;
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "./%s", FOLDER_NAME);
    
    
    //Tiempos
    struct timespec inicio, fin;

    clock_gettime(CLOCK_REALTIME, &inicio);

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
            filenames[fileCount++] = strdup(fullpath);
        }
    }
    closedir(dir);

    if (fileCount == 0) {
        printf("No se encontraron archivos .txt en '%s'\n", FOLDER_NAME);
        return 0;
    }

    int frequencies[256] = {0};
    for (int i = 0; i < fileCount; i++) {
        FILE *input = fopen(filenames[i], "rb");
        if (input) {
            unsigned char symbol;
            while (fread(&symbol, sizeof(unsigned char), 1, input) == 1) {
                frequencies[symbol]++;
            }
            fclose(input);
        }
    }
    
    // Agregar FILE_SEP forzado
    for (int j = 0; DELIMITER[j]; j++) {
        unsigned char ch = (unsigned char)DELIMITER[j];
        if (frequencies[ch] == 0) {
            frequencies[ch] = 1;
        }
    }


    HuffmanNode *root = buildHuffmanTree(frequencies);
    if (!root) {
        fprintf(stderr, "Error construyendo el árbol de Huffman\n");
        return 1;
    }

    char *codes[256] = {NULL};
    char currentCode[MAX_CODE_LENGTH] = {0};
    generateCodes(root, codes, currentCode, 0);

    FILE *codesFile = fopen("codes.txt", "w");
    if (codesFile) {
        saveCodesToFile(codes, codesFile);
        fclose(codesFile);
    }

    freeHuffmanTree(root);
    for (int i = 0; i < 256; i++) free(codes[i]);

    // FORK: one child per file
    for (int i = 0; i < fileCount; i++) {
        pid_t pid = fork();
        if (pid == 0) { // child
            char tempFilename[64];
            snprintf(tempFilename, sizeof(tempFilename), "tmp_out_%d.bin", i);
            compressFileChild(filenames[i], tempFilename);
            exit(0);
        } else if (pid < 0) {
            perror("Error creando proceso hijo");
            exit(EXIT_FAILURE);
        }
    }

    // Parent: wait for all children
    for (int i = 0; i < fileCount; i++) {
        wait(NULL);
    }

    mergeTemporaryFiles(fileCount);

    printf("\nCompresión completada:\n");
    printf("- %d archivos comprimidos\n", fileCount);

    for (int i = 0; i < fileCount; i++) free(filenames[i]);
    
    clock_gettime(CLOCK_REALTIME, &fin);

    // 3. Calcular la diferencia en segundos y nanosegundos
    long diferencia_segundos = fin.tv_sec - inicio.tv_sec;
    long diferencia_nanosegundos = fin.tv_nsec - inicio.tv_nsec;

    // 4. Convertir todo a nanosegundos (1 segundo = 1,000,000,000 ns)
    double tiempo_total_ns = (diferencia_segundos * 1e9) + diferencia_nanosegundos;

    // Mostrar resultados
    printf("Tiempo total: %.0f nanosegundos\n", tiempo_total_ns);
    printf("Equivale a:\n");
    printf("- %.9f segundos\n", tiempo_total_ns / 1e9);
    printf("- %.3f milisegundos\n", tiempo_total_ns / 1e6);


    return 0;
}
