
//HAGO ESTE DE UN ARCHIVOO INSPIRADO EL DE INDIVIDUALES 
// VERSION 2 CON TODOOO



#define _POSIX_C_SOURCE 199309L

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>  // mkdir()
#include <stdint.h>   

// Para no copiar todo el code de compres, lo incluimos como cabecera de nuevo
#define main compress_serial_main
#include "../compress.c"
#undef main


static void compressToBinaryFile(FILE *in, FILE *out, char *codes[256]) {
    rewind(in);
    unsigned char buffer = 0;
    int bitCount = 0, c;
    while ((c = fgetc(in)) != EOF) {
        char *code = codes[(unsigned char)c];
        for (int i = 0; code[i]; i++) {



            buffer = (buffer << 1) | (code[i] == '1');
            if (++bitCount == 8) {
                fwrite(&buffer,1,1,out);
                buffer = bitCount = 0;



            }
        }
    }
    if (bitCount) {



        buffer <<= (8 - bitCount);
        fwrite(&buffer,1,1,out);
    }
}


void compressFile(const char *inName, const char *outName) {
    FILE *in = fopen(inName, "rb");



    if (!in) { perror(inName); return; }

    int freq[256];
    calculateFrequencies(in, freq);

    HuffmanNode *root = buildHuffmanTree(freq);

    char *codes[256] = {0};
    char codeBuf[MAX_CODE_LENGTH];



    generateCodes(root, codes, codeBuf, 0);

    FILE *out = fopen(outName, "wb");
    if (!out) { perror(outName); fclose(in); freeHuffmanTree(root); return; }

    compressToBinaryFile(in, out, codes);




    fclose(in);
    fclose(out);




    freeHuffmanTree(root);
    for (int i = 0; i < 256; i++) free(codes[i]);
}



// ESTO ES NUEVO PARA ESTA VERSION : juntamos cada .bin temporal en un UNICO comp_todos.bin
static pthread_mutex_t all_mutex = PTHREAD_MUTEX_INITIALIZER;

void compressFileToAll(const char *inName, FILE *allOut) {
    //   1 Abrir input y calcular frecuencias
    FILE *in = fopen(inName, "rb");
    if (!in) { perror(inName); return; }
    int freq[256] = {0};
    calculateFrequencies(in, freq);

    // 2 Construir tree y generar codigo
    HuffmanNode *root = buildHuffmanTree(freq);
    char *codes[256] = {0};
    char codeBuf[MAX_CODE_LENGTH];
    generateCodes(root, codes, codeBuf, 0);

    // 3  nombre y tamaño de datos comprimidos
    //    
    //    Para no usar tmp, vamos a comprimir directo al final,
    //    pero primero guardamos el nombree
    pthread_mutex_lock(&all_mutex);
    uint16_t nameLen = strlen(inName);
    fwrite(&nameLen, sizeof(nameLen), 1, allOut);
    fwrite(inName, 1, nameLen, allOut);

    // 4) Serializar la tabla de codigos
    //    how many simb  hay:
    uint16_t numSyms = 0;


    for (int i = 0; i < 256; i++) if (codes[i]) numSyms++;



    fwrite(&numSyms, sizeof(numSyms), 1, allOut);

    //    Para cada sim con codig:
    for (int i = 0; i < 256; i++) {
        if (!codes[i]) continue;
        uint8_t symbol  = (uint8_t)i;
        uint8_t codeLen = strlen(codes[i]);          
        int     bytesLen= (codeLen + 7) / 8;           
        uint8_t buf[32] = {0};

        // Empaquetar 0 y 
        for (int b = 0; b < codeLen; b++) {
            int byteIdx = b/8, bitIdx = 7-(b%8);
            if (codes[i][b]=='1') buf[byteIdx] |= (1<<bitIdx);
        }

        fwrite(&symbol,   sizeof(symbol), 1, allOut);
        fwrite(&codeLen,  sizeof(codeLen),1, allOut);
        fwrite(buf,       1, bytesLen,     allOut);
    }

    // 5) Ahora saber cuanto bytes compimi se vaa a escribi.
    //    Para ello, vamos a comprimiren memoria” a un buffer dina:
    //    (reabrimos input y usamos compressToBinaryFile en un FILE* a memoria)
    rewind(in);
    // crear un stream de memoria con open_memstream
    char *memBuf = NULL;
    size_t memSize = 0;
    FILE *memf = open_memstream(&memBuf, &memSize);
    compressToBinaryFile(in, memf, codes);
    fclose(memf);
    // memBuf contiene memSize bytes comprimidos

    // 7(Volcar el tamaño y los bytes comprimidos
    uint32_t dataSize = memSize;
    fwrite(&dataSize, sizeof(dataSize), 1, allOut);
    fwrite(memBuf, 1, dataSize, allOut);

    pthread_mutex_unlock(&all_mutex);

    // 7) Limpiar
    free(memBuf);
    fclose(in);
    freeHuffmanTree(root);
    for (int i = 0; i < 256; i++) free(codes[i]);
}


// ---------------------------------------------
// Hilo que llama a compressFileToAll()
typedef struct { const char *inFile; } ThreadArg;

extern FILE *f_all;  // declarada abajo

void *threadFunc(void *arg) {




    ThreadArg *t = arg;
    printf("[TID %lu] INICIO: %s\n",
           (unsigned long)pthread_self(), t->inFile);
    compressFileToAll(t->inFile, f_all);
    printf("[TID %lu] FIN   : %s\n",
           (unsigned long)pthread_self(), t->inFile);
    return NULL;




}





FILE *f_all;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <libro1.txt> [libro2.txt...]\n", argv[0]);
        return 1;
    }

    mkdir("salida", 0755);


    



    f_all = fopen("salida/comp_todos.bin", "wb");
    if (!f_all) { perror("comp_todos.bin"); return 1; }

    int n = argc - 1;
    pthread_t *threads = malloc(n * sizeof(pthread_t));
    ThreadArg *args    = malloc(n * sizeof(ThreadArg));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < n; i++) {
        args[i].inFile = argv[i+1];
        if (pthread_create(&threads[i], NULL, threadFunc, &args[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long dur_ns = (t1.tv_sec - t0.tv_sec)*1000000000L + (t1.tv_nsec - t0.tv_nsec);
    printf("Tiempo total paralelo: %ld ns\n", dur_ns);
    printf("Archivos procesados: %d\n", n);

    fclose(f_all);  
    

    free(threads);
    free(args);
    return 0;
}
