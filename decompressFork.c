#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_CODE_LENGTH 256
#define DELIMITER "FILE_SEP"
#define OUTPUT_DIR "descomprimidos"

typedef struct HuffmanNode {
    unsigned char symbol;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

// Crear directorio si no existe
void createOutputDir() {
    struct stat st = {0};
    if (stat(OUTPUT_DIR, &st) == -1) {
        mkdir(OUTPUT_DIR, 0700);
    }
}

HuffmanNode* rebuildTreeFromCodes(FILE *codesFile) {
    HuffmanNode *root = calloc(1, sizeof(HuffmanNode));
    char line[MAX_CODE_LENGTH + 10];
    
    while (fgets(line, sizeof(line), codesFile)) {
        int ascii;
        char code[MAX_CODE_LENGTH];
        if (sscanf(line, "%d:%s", &ascii, code) != 2) continue;
        
        HuffmanNode *current = root;
        for (int i = 0; code[i]; i++) {
            if (code[i] == '0') {
                if (!current->left) current->left = calloc(1, sizeof(HuffmanNode));
                current = current->left;
            } else {
                if (!current->right) current->right = calloc(1, sizeof(HuffmanNode));
                current = current->right;
            }
        }
        current->symbol = (unsigned char)ascii;
    }
    return root;
}

void decompressChunk(const char *chunk, int chunkLen, HuffmanNode *root, int fileIndex) {
    char output_path[PATH_MAX];
    snprintf(output_path, sizeof(output_path), "%s/archivo_%d.txt", OUTPUT_DIR, fileIndex);
    FILE *out = fopen(output_path, "w");
    if (!out) {
        perror("Error creando archivo de salida");
        exit(EXIT_FAILURE);
    }

    HuffmanNode *current = root;
    for (int i = 0; i < chunkLen; i++) {
        if (chunk[i] == '0') current = current->left;
        else if (chunk[i] == '1') current = current->right;
        else continue;

        if (current && !current->left && !current->right) {
            fputc(current->symbol, out);
            current = root;
        }
    }

    fclose(out);
    exit(0); // Child exits after writing its output
}

void decompressToMultipleFiles(FILE *input, HuffmanNode *root) {
    createOutputDir();

    char *fileBuffer = NULL;
    long fileSize;
    
    fseek(input, 0, SEEK_END);
    fileSize = ftell(input);
    rewind(input);

    fileBuffer = malloc(fileSize + 1);
    if (!fileBuffer) {
        perror("Error reservando memoria");
        exit(EXIT_FAILURE);
    }

    fread(fileBuffer, 1, fileSize, input);
    fileBuffer[fileSize] = '\0';

    char *start = fileBuffer;
    int fileIndex = 1;
    pid_t pids[100];  // Support up to 100 parallel files
    int pidCount = 0;

    // Skip initial delimiter if it's right at the beginning
    if (strncmp(start, DELIMITER, strlen(DELIMITER)) == 0) {
        start += strlen(DELIMITER);
    }

    while (1) {
        char *sep = strstr(start, DELIMITER);
        if (!sep) break;

        int chunkLen = sep - start;
        if (chunkLen > 0) {
            char *chunk = malloc(chunkLen + 1);
            strncpy(chunk, start, chunkLen);
            chunk[chunkLen] = '\0';

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                decompressChunk(chunk, chunkLen, root, fileIndex);
            } else {
                pids[pidCount++] = pid;
                fileIndex++;
            }

            free(chunk);
        }

        start = sep + strlen(DELIMITER);
    }

    // Handle final chunk (after last delimiter), if non-empty
    if (*start) {
        int chunkLen = strlen(start);
        if (chunkLen > 0) {
            char *chunk = strdup(start);
            pid_t pid = fork();
            if (pid == 0) {
                decompressChunk(chunk, chunkLen, root, fileIndex);
            } else {
                pids[pidCount++] = pid;
            }
            free(chunk);
        }
    }

    // Wait for all child processes
    for (int i = 0; i < pidCount; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free(fileBuffer);
}


void freeTree(HuffmanNode *root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

int main() {
    FILE *codesFile = fopen("codes.txt", "r");
    if (!codesFile) {
        perror("Error abriendo codes.txt");
        return 1;
    }
    
    FILE *compressedFile = fopen("textoComprimido.txt", "r");
    if (!compressedFile) {
        perror("Error abriendo textoComprimido.txt");
        fclose(codesFile);
        return 1;
    }
    
    HuffmanNode *root = rebuildTreeFromCodes(codesFile);
    decompressToMultipleFiles(compressedFile, root);
    
    freeTree(root);
    fclose(codesFile);
    fclose(compressedFile);
    
    printf("Descompresión completada. Archivos guardados en el directorio '%s/'\n", OUTPUT_DIR);
    return 0;
}