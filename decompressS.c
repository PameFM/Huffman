#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

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

void decompressToMultipleFiles(FILE *input, HuffmanNode *root) {
    createOutputDir();
    
    HuffmanNode *current = root;
    char buffer[4096];
    int delim_pos = 0;
    int file_count = 0;
    FILE *current_output = NULL;
    char output_path[PATH_MAX];
    
    while (1) {
        int c = fgetc(input);
        if (c == EOF) break;
        
        // Manejo del delimitador
        if (c == DELIMITER[delim_pos]) {
            delim_pos++;
            if (DELIMITER[delim_pos] == '\0') {
                // Cerrar archivo anterior si existe
                if (current_output) fclose(current_output);
                
                // Crear nuevo archivo de salida
                snprintf(output_path, sizeof(output_path), "%s/archivo_%d.txt", OUTPUT_DIR, ++file_count);
                current_output = fopen(output_path, "w");
                if (!current_output) {
                    perror("Error creando archivo de salida");
                    break;
                }
                
                delim_pos = 0;
                current = root;
                continue;
            }
        } else {
            // Procesar bits normales
            if (delim_pos > 0) {
                fseek(input, -delim_pos-1, SEEK_CUR);
                delim_pos = 0;
                c = fgetc(input);
            }
            
            if (c == '0') current = current->left;
            else if (c == '1') current = current->right;
            
            if (current && !current->left && !current->right) {
                if (current_output) fputc(current->symbol, current_output);
                current = root;
            }
        }
    }
    
    if (current_output) fclose(current_output);
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