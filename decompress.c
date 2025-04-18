#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CODE_LENGTH 256

typedef struct HuffmanNode {
    unsigned char symbol;
    struct HuffmanNode *left;  // Puntero a hijo izquierdo (bit 0)
    struct HuffmanNode *right; // Puntero a hijo derecho (bit 1)
} HuffmanNode;

// Reconstruye el árbol de Huffman a partir de los códigos
HuffmanNode *rebuildHuffmanTree(FILE *codesFile) {
    HuffmanNode *root = malloc(sizeof(HuffmanNode));
    root->left = root->right = NULL;
    root->symbol = '\0';

    int asciiCode;
    char code[MAX_CODE_LENGTH];
    while (fscanf(codesFile, "%d:%s\n", &asciiCode, code) != EOF) {
        HuffmanNode *current = root;
        for (int i = 0; code[i] != '\0'; i++) {
            if (code[i] == '0') {
                if (current->left == NULL) {
                    current->left = malloc(sizeof(HuffmanNode));
                    current->left->left = current->left->right = NULL;
                    current->left->symbol = '\0';
                }
                current = current->left;
            } else { // code[i] == '1'
                if (current->right == NULL) {
                    current->right = malloc(sizeof(HuffmanNode));
                    current->right->left = current->right->right = NULL;
                    current->right->symbol = '\0';
                }
                current = current->right;
            }
        }
        current->symbol = (unsigned char)asciiCode; // Asigna el símbolo al nodo hoja
    }
    return root;
}

// Descomprime el archivo de bits usando el árbol de Huffman
void decompressFile(FILE *input, FILE *output, HuffmanNode *root) {
    HuffmanNode *current = root;
    char bit;
    while ((bit = fgetc(input)) != EOF) {
        if (bit == '0') {
            current = current->left;
        } else if (bit == '1') {
            current = current->right;
        }

        // Si llegamos a una hoja, escribimos el símbolo
        if (current->left == NULL && current->right == NULL) {
            fputc(current->symbol, output);
            current = root; // Reiniciamos al nodo raíz
        }
    }
}

// Libera la memoria del árbol
void freeTree(HuffmanNode *root) {
    if (root == NULL) return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

int main() {
    const char *compressedFilename = "textoComprimido.txt";
    const char *codesFilename = "codes.txt";
    const char *decompressedFilename = "textoDescomprimido.txt";

    // Abrir archivos
    FILE *codesFile = fopen(codesFilename, "r");
    if (codesFile == NULL) {
        perror("Error al abrir codes.txt");
        return 1;
    }

    FILE *compressedFile = fopen(compressedFilename, "r");
    if (compressedFile == NULL) {
        perror("Error al abrir output.txt");
        return 1;
    }

    FILE *decompressedFile = fopen(decompressedFilename, "w");
    if (decompressedFile == NULL) {
        perror("Error al abrir decompressed.txt");
        return 1;
    }

    // Reconstruir el árbol y descomprimir
    HuffmanNode *root = rebuildHuffmanTree(codesFile);
    decompressFile(compressedFile, decompressedFile, root);

    // Liberar recursos
    fclose(codesFile);
    fclose(compressedFile);
    fclose(decompressedFile);
    freeTree(root);

    printf("Descompresión completada. Resultado en '%s'.\n", decompressedFilename);
    return 0;
}