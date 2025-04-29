#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <limits.h>

// Aqui se guardaran los archivos descomprimidos
#define OUT_DIR "salida/descomp"

// -------------------------------
// Estructura de tarea para cada archivo
typedef struct {
    char    *name;   // Nombre de archivo original
    uint16_t numSyms;  //   Canti de simbolos
    uint8_t *codesBuf;   // Buffer serializado de c
    uint32_t dataSize;   //   Tamaño de datos comprimidos
    uint8_t *dataBuf;  // Buffer de datos comprimidos
} Task;

// Nodo del arbol Huffman
typedef struct Node {


    unsigned char symbol;
    struct Node *left, *right;
} Node;

// Crear nodo vacío
static Node* newNode() {
    Node *n = malloc(sizeof(Node));
    if (!n) { perror("malloc"); exit(EXIT_FAILURE); }
    n->symbol = 0;
    n->left = n->right = NULL;
    return n;
}

// Insertar un codigo binario en el tree de Huffman
static void insertCode(Node *root,
                       unsigned char symbol,
                       uint8_t *bits,
                       uint8_t codeLen) {
    Node *cur = root;
    for (int i = 0; i < codeLen; i++) {

        int byteIdx = i/8, bitIdx = 7 - (i % 8);
        int bit = (bits[byteIdx] >> bitIdx) & 1;
        if (bit) {
            if (!cur->right) cur->right = newNode();
            cur = cur->right;
        } else {
            if (!cur->left) cur->left = newNode();
            cur = cur->left;
        }


    }
    cur->symbol = symbol;
}

// Liberar memoria del tree de Huffman
static void freeTree(Node *r) {
    if (!r) return;

    freeTree(r->left);
    freeTree(r->right);
    free(r);
}

// Función ejecutada por cada hilo para descomprimir
static void *decompressThread(void *arg) {
    Task *task = (Task *)arg;

    printf("[TID %lu] INICIO DECOMP: %s\n", (unsigned long)pthread_self(), task->name);

    // 1) Reconstruir el arbolillo Huffman
    Node *root = newNode();
    uint8_t *p = task->codesBuf;
    for (int i = 0; i < task->numSyms; i++) {

        uint8_t symbol = *p++;
        uint8_t codeLen = *p++;
        int bytesLen  = (codeLen + 7) / 8;
        insertCode(root, symbol, p, codeLen);
        p += bytesLen;


    }

    // 2) Preparar ruta de salida
    char outPath[PATH_MAX];
    const char *base = strrchr(task->name, '/');

    base = base ? base + 1 : task->name;

    snprintf(outPath, sizeof(outPath), "%s/%s", OUT_DIR, base);

    // 3) Descomprimir datos en el archivo de salida
    FILE *out = fopen(outPath, "wb");
    if (!out) {
        perror(outPath);
    } else {
        Node *cur = root;
        for (uint32_t i = 0; i < task->dataSize; i++) {


            uint8_t byte = task->dataBuf[i];
            for (int b = 7; b >= 0; b--) {
                int bit = (byte >> b) & 1;
                cur = bit ? cur->right : cur->left;
                if (!cur->left && !cur->right) {
                    fputc(cur->symbol, out);
                    cur = root;
                }
            }
        }


        fclose(out);
    }

    // Final
    printf("[TID %lu] FIN DECOMP  : %s\n", (unsigned long)pthread_self(), task->name);

    // 4) Liberar recursos de la tarea
    freeTree(root);
    free(task->codesBuf);
    free(task->dataBuf);
    free(task->name);
    free(task);
    return NULL;
}

int main() {
    const char *inputBin = "salida/comp_todos.bin";
    FILE *in = fopen(inputBin, "rb");
    if (!in) {
        perror(inputBin);
        return 1;
    }

    // Crear directorio de salida para los archivos descomprimidos
    mkdir(OUT_DIR, 0755);

    // Vector dina de tareas
    Task **tasks = NULL;
    size_t taskCount = 0;

    // Leer cada sec de comp_todos.bin y construir tareas
    while (1) {
        uint16_t nameLen;
        if (fread(&nameLen, sizeof(nameLen), 1, in) != 1) break;

        char *name = malloc(nameLen + 1);
        fread(name, 1, nameLen, in);
        name[nameLen] = '\0';

        uint16_t numSyms;
        fread(&numSyms, sizeof(numSyms), 1, in);

        // Leer tabla de codes
        size_t maxCodesSize = numSyms * (2 + (256 + 7) / 8);
        uint8_t *codesBuf = malloc(maxCodesSize);
        size_t cbPos = 0;
        for (int i = 0; i < numSyms; i++) {


            uint8_t symbol, codeLen;
            fread(&symbol, sizeof(symbol), 1, in);
            fread(&codeLen, sizeof(codeLen), 1, in);
            int bytesLen = (codeLen + 7) / 8;
            codesBuf[cbPos++] = symbol;
            codesBuf[cbPos++] = codeLen;
            fread(codesBuf + cbPos, 1, bytesLen, in);
            cbPos += bytesLen;


        }

        // Leer bloque comprimido
        uint32_t dataSize;
        fread(&dataSize, sizeof(dataSize), 1, in);
        uint8_t *dataBuf = malloc(dataSize);
        fread(dataBuf, 1, dataSize, in);

        // Almacenar tarea
        tasks = realloc(tasks, (taskCount + 1) * sizeof(Task*));
        Task *t = malloc(sizeof(Task));
        t->name     = name;
        t->numSyms  = numSyms;
        t->codesBuf = codesBuf;
        t->dataSize = dataSize;
        t->dataBuf  = dataBuf;
        tasks[taskCount++] = t;
    }
    fclose(in);

    // Lanzar hilos para cada tarea
    pthread_t *threads = malloc(taskCount * sizeof(pthread_t));
    for (size_t i = 0; i < taskCount; i++) {
        if (pthread_create(&threads[i], NULL, decompressThread, tasks[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Esperar la termina de todos los hilos
    for (size_t i = 0; i < taskCount; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tasks);
    printf("Descompresion paralela completada (%zu archivos)\n", taskCount);
    return 0;
}
