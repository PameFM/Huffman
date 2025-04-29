#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h> 

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

int readBit(FILE *input, unsigned char *buffer, int *bitPos) {
    if (*bitPos == 0) {
        if (fread(buffer, 1, 1, input) != 1) {
            return EOF;
        }
    }
    
    int bit = (*buffer >> (7 - *bitPos)) & 1;
    *bitPos = (*bitPos + 1) % 8;
    return bit;
}

void decompressToMultipleFiles(FILE *input, HuffmanNode *root) {
    createOutputDir();
    
    HuffmanNode *current = root;
    unsigned char buffer = 0;
    int bitPos = 0;
    int file_count = 0;
    FILE *current_output = NULL;
    char output_path[PATH_MAX];
    
    // Buffer para el delimitador
    char delim_buffer[strlen(DELIMITER)];
    int delim_index = 0;
    
    while (1) {
        int bit = readBit(input, &buffer, &bitPos);
        if (bit == EOF) break;
        
        // Manejar el bit actual
        if (bit == 0) {
            current = current->left;
        } else {
            current = current->right;
        }
        
        // Si llegamos a una hoja
        if (current && !current->left && !current->right) {
            // Verificar si estamos en medio de la detección del delimitador
            if (delim_index > 0) {
                // Si el carácter no coincide con el delimitador
                if (current->symbol != DELIMITER[delim_index]) {
                    // Escribir los caracteres acumulados
                    for (int i = 0; i < delim_index; i++) {
                        if (current_output) {
                            fputc(DELIMITER[i], current_output);
                        }
                    }
                    // Escribir el carácter actual
                    if (current_output) {
                        fputc(current->symbol, current_output);
                    }
                    delim_index = 0;
                } else {
                    // Continuar verificando el delimitador
                    delim_buffer[delim_index++] = current->symbol;
                    
                    // Si completamos el delimitador
                    if (delim_index == strlen(DELIMITER)) {
                        // Cerrar archivo anterior si existe
                        if (current_output) {
                            fclose(current_output);
                        }
                        
                        // Crear nuevo archivo de salida
                        snprintf(output_path, sizeof(output_path), "%s/archivo_%d.txt", OUTPUT_DIR, ++file_count);
                        current_output = fopen(output_path, "w");
                        if (!current_output) {
                            perror("Error creando archivo de salida");
                            break;
                        }
                        
                        delim_index = 0;
                    }
                }
            } else {
                // Verificar si el carácter es el inicio del delimitador
                if (current->symbol == DELIMITER[0]) {
                    delim_buffer[0] = current->symbol;
                    delim_index = 1;
                } else {
                    // Escribir el carácter normal
                    if (current_output) {
                        fputc(current->symbol, current_output);
                    }
                }
            }
            
            current = root;
        }
    }
    
    if (current_output) {
        fclose(current_output);
    }
    
    // Si quedaron caracteres del delimitador sin procesar
    if (delim_index > 0 && file_count > 0) {
        current_output = fopen(output_path, "a"); // Abrir el último archivo en modo append
        if (current_output) {
            for (int i = 0; i < delim_index; i++) {
                fputc(delim_buffer[i], current_output);
            }
            fclose(current_output);
        }
    }
}

void freeTree(HuffmanNode *root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

int main() {
    struct timespec inicio, fin;
    clock_gettime(CLOCK_REALTIME, &inicio);

    FILE *codesFile = fopen("codes.txt", "r");
    if (!codesFile) {
        perror("Error abriendo codes.txt");
        return 1;
    }
    
    FILE *compressedFile = fopen("textoComprimido.bin", "rb"); // Modo binario
    if (!compressedFile) {
        perror("Error abriendo textoComprimido.bin");
        fclose(codesFile);
        return 1;
    }
    
    HuffmanNode *root = rebuildTreeFromCodes(codesFile);
    decompressToMultipleFiles(compressedFile, root);

    clock_gettime(CLOCK_REALTIME, &fin);

    long diferencia_segundos = fin.tv_sec - inicio.tv_sec;
    long diferencia_nanosegundos = fin.tv_nsec - inicio.tv_nsec;
    double tiempo_total_ns = (diferencia_segundos * 1e9) + diferencia_nanosegundos;
    
    printf("Descompresión completada. Archivos guardados en el directorio '%s/'\n", OUTPUT_DIR);
    
    printf("\nTiempo total: %.0f nanosegundos\n", tiempo_total_ns);
    printf("Equivale a:\n");
    printf("- %.9f segundos\n", tiempo_total_ns / 1e9);
    printf("- %.3f milisegundos\n", tiempo_total_ns / 1e6);

    freeTree(root);
    fclose(codesFile);
    fclose(compressedFile);
    
    return 0;
}