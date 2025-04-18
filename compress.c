#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_TREE_NODES 256
#define MAX_CODE_LENGTH 256

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

// Crear un nuevo nodo
HuffmanNode *createNode(unsigned char symbol, int frequency) {
    HuffmanNode *node = (HuffmanNode *)malloc(sizeof(HuffmanNode));
    node->symbol = symbol;
    node->frequency = frequency;
    node->left = node->right = NULL;
    return node;
}

// Inicializar la cola de prioridad
void initPriorityQueue(PriorityQueue *pq) {
    pq->size = 0;
}

// Insertar nodo en la cola de prioridad
void enqueue(PriorityQueue *pq, HuffmanNode *node) {
    int i = pq->size;
    while (i > 0 && pq->nodes[(i - 1) / 2]->frequency > node->frequency) {
        pq->nodes[i] = pq->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    pq->nodes[i] = node;
    pq->size++;
}

// Extraer el nodo con menor frecuencia
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

// Construir el árbol de Huffman
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

    return dequeue(&pq);
}

// Generar códigos Huffman
void generateCodes(HuffmanNode *root, char **codes, char *currentCode, int depth) {
    if (root == NULL) return;

    if (root->left == NULL && root->right == NULL) {
        currentCode[depth] = '\0';
        codes[root->symbol] = strdup(currentCode);
        return;
    }

    currentCode[depth] = '0';
    generateCodes(root->left, codes, currentCode, depth + 1);

    currentCode[depth] = '1';
    generateCodes(root->right, codes, currentCode, depth + 1);
}

// Liberar memoria del árbol
void freeHuffmanTree(HuffmanNode *root) {
    if (root == NULL) return;
    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}

// Calcular frecuencias de caracteres
void calculateFrequencies(FILE *input, int *frequencies) {
    memset(frequencies, 0, 256 * sizeof(int));
    unsigned char symbol;
    while (fread(&symbol, sizeof(unsigned char), 1, input)) {
        frequencies[symbol]++;
    }
    rewind(input);
}

// Comprimir a archivo de texto con bits legibles
void compressToTextFile(FILE *input, FILE *output, char **codes) {
    unsigned char symbol;
    while (fread(&symbol, sizeof(unsigned char), 1, input)) {
        fputs(codes[symbol], output);  // Escribe los bits como texto
    }
}

// Guardar códigos en archivo
void saveCodesToFile(char **codes, FILE *output) {
    for (int i = 0; i < 256; i++) {
        if (codes[i] != NULL) {
            fprintf(output, "%d:%s\n", i, codes[i]);
        }
    }
}

int main() {
    const char *inputFilename = "entrada.txt";
    const char *outputFilename = "textoComprimido.txt";  // Archivo de texto con bits
    const char *codesFilename = "codes.txt";

    FILE *inputFile = fopen(inputFilename, "rb");
    if (inputFile == NULL) {
        perror("Error al abrir input.txt");
        return 1;
    }

    int frequencies[256];
    calculateFrequencies(inputFile, frequencies);

    HuffmanNode *root = buildHuffmanTree(frequencies);

    char *codes[256] = { NULL };
    char currentCode[MAX_CODE_LENGTH];
    generateCodes(root, codes, currentCode, 0);

    // Guardar códigos
    FILE *codesFile = fopen(codesFilename, "w");
    if (codesFile == NULL) {
        perror("Error al abrir codes.txt");
        return 1;
    }
    saveCodesToFile(codes, codesFile);
    fclose(codesFile);

    // Comprimir a texto legible
    FILE *outputFile = fopen(outputFilename, "w");
    if (outputFile == NULL) {
        perror("Error al abrir output.txt");
        return 1;
    }
    compressToTextFile(inputFile, outputFile, codes);
    fclose(outputFile);
    fclose(inputFile);

    printf("Compresión completada:\n");
    printf("- Códigos guardados en '%s'\n", codesFilename);
    printf("- Texto comprimido (bits) en '%s'\n", outputFilename);

    // Liberar memoria
    freeHuffmanTree(root);
    for (int i = 0; i < 256; i++) {
        free(codes[i]);
    }

    return 0;
}