#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CODE_LENGTH 256
#define DELIMITER "FILE_SEP"
#define OUTPUT_FOLDER "descomprimidos"
#define MAX_SYMBOLS 4096

typedef struct HuffmanNode {
    unsigned char symbol;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

HuffmanNode* createNode(unsigned char symbol) {
    HuffmanNode *node = (HuffmanNode *)malloc(sizeof(HuffmanNode));
    node->symbol = symbol;
    node->left = node->right = NULL;
    return node;
}

void insertCode(HuffmanNode *root, const char *code, unsigned char symbol) {
    HuffmanNode *current = root;
    for (int i = 0; code[i]; i++) {
        if (code[i] == '0') {
            if (!current->left) current->left = createNode(0);
            current = current->left;
        } else if (code[i] == '1') {
            if (!current->right) current->right = createNode(0);
            current = current->right;
        }
    }
    current->symbol = symbol;
}

HuffmanNode* buildHuffmanTreeFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error abriendo codes.txt");
        exit(EXIT_FAILURE);
    }
    HuffmanNode *root = createNode(0);
    int symbol;
    char code[MAX_CODE_LENGTH];
    while (fscanf(fp, "%d:%s\n", &symbol, code) == 2) {
        insertCode(root, code, (unsigned char)symbol);
    }
    fclose(fp);
    return root;
}

void freeHuffmanTree(HuffmanNode *root) {
    if (!root) return;
    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}

void writeFile(int fileIndex, char *data, int size) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/archivo_%d.txt", OUTPUT_FOLDER, fileIndex);

    FILE *out = fopen(filename, "wb");
    if (!out) {
        perror("Error creando archivo de salida");
        exit(EXIT_FAILURE);
    }
    fwrite(data, 1, size, out);
    fclose(out);
}

bool bufferMatchesDelimiter(char *buffer) {
    return (memcmp(buffer, DELIMITER, strlen(DELIMITER)) == 0);
}

int main() {
    HuffmanNode *tree = buildHuffmanTreeFromFile("codes.txt");

    FILE *input = fopen("textoComprimido.bin", "rb");
    if (!input) {
        perror("Error abriendo textoComprimido.bin");
        exit(EXIT_FAILURE);
    }
    fseek(input, 0, SEEK_END);
    long fileSize = ftell(input);
    rewind(input);
    
    //Tiempos
    struct timespec inicio, fin;

    clock_gettime(CLOCK_REALTIME, &inicio);

    unsigned char *compressedData = malloc(fileSize);
    fread(compressedData, 1, fileSize, input);
    fclose(input);

    struct stat st = {0};
    if (stat(OUTPUT_FOLDER, &st) == -1) {
        mkdir(OUTPUT_FOLDER, 0700);
    }

    HuffmanNode *current = tree;
    char outputBuffer[MAX_SYMBOLS];
    int outputBufferPos = 0;
    char lookaheadBuffer[64] = {0};
    int lookaheadSize = 0;

    int fileIndex = 0;
    bool started = false;
    int bitPos = 0;
    int delimiterLength = strlen(DELIMITER);

    while (bitPos < fileSize * 8) {
        int byteIndex = bitPos / 8;
        int bitIndex = 7 - (bitPos % 8);
        int bit = (compressedData[byteIndex] >> bitIndex) & 1;

        if (bit == 0) current = current->left;
        else current = current->right;

        if (!current->left && !current->right) {
            unsigned char decoded = current->symbol;

            memmove(lookaheadBuffer, lookaheadBuffer + 1, sizeof(lookaheadBuffer) - 1);
            lookaheadBuffer[sizeof(lookaheadBuffer) - 1] = decoded;
            if (lookaheadSize < sizeof(lookaheadBuffer)) lookaheadSize++;

            if (lookaheadSize >= delimiterLength && bufferMatchesDelimiter(&lookaheadBuffer[sizeof(lookaheadBuffer) - delimiterLength])) {
                if (!started) {
                    started = true; // Primer delimitador, no guardar
                } else {
                    if (outputBufferPos > 0) {
                        // Copiar contenido seguro
                        int dataSize = outputBufferPos;
                        char *dataCopy = malloc(dataSize);
                        memcpy(dataCopy, outputBuffer, dataSize);

                        int thisFileIndex = ++fileIndex;

                        pid_t pid = fork();
                        if (pid == 0) { // hijo
                            writeFile(thisFileIndex, dataCopy, dataSize);
                            free(dataCopy);
                            free(compressedData);
                            freeHuffmanTree(tree);
                            exit(0);
                        }
                        free(dataCopy); // padre libera
                    }
                }
                outputBufferPos = 0;
                memset(lookaheadBuffer, 0, sizeof(lookaheadBuffer));
                lookaheadSize = 0;
                current = tree;
                bitPos++;
                continue;
            }

            if (started) {
                if (lookaheadSize > delimiterLength) {
                    outputBuffer[outputBufferPos++] = lookaheadBuffer[sizeof(lookaheadBuffer) - delimiterLength - 1];
                    if (outputBufferPos >= MAX_SYMBOLS) {
                        fprintf(stderr, "Error: Buffer overflow\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            current = tree;
        }

        bitPos++;
    }

    // Guardar último archivo
    if (outputBufferPos > 0) {
        int dataSize = outputBufferPos;
        char *dataCopy = malloc(dataSize);
        memcpy(dataCopy, outputBuffer, dataSize);

        int thisFileIndex = ++fileIndex;

        pid_t pid = fork();
        if (pid == 0) {
            writeFile(thisFileIndex, dataCopy, dataSize);
            free(dataCopy);
            free(compressedData);
            freeHuffmanTree(tree);
            exit(0);
        }
        free(dataCopy);
    }

    while (wait(NULL) > 0);

    printf("Descompresión completada: %d archivos.\n", fileIndex);

    free(compressedData);
    freeHuffmanTree(tree);
    
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
