// gcc compress_pthread_debug.c -o compress_pthread_debug -lpthread
#define _POSIX_C_SOURCE 199309L




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>  // para mkdir()

// Para no tomar todo el codigo de compress.c, lo renombramos por si acaso 
// y lo incluimos como un archivo de cabecera

// se renombr el main de compress.c
#define main compress_serial_main
#include "compress.c"
#undef main

pthread_mutex_t all_mutex = PTHREAD_MUTEX_INITIALIZER;  






void compressFileToAll(const char *inName, FILE *allOut) {
    // Genera un nombre temporal


    char tmpName[512];
    snprintf(tmpName, sizeof(tmpName), "salida/.tmp_%ld.bin", random());
    compressFile(inName, tmpName);  
    

   
    FILE *tmp = fopen(tmpName, "rb");
    if (!tmp) return;
    pthread_mutex_lock(&all_mutex);
    int c;



    while ((c = fgetc(tmp)) != EOF) fputc(c, allOut);
    pthread_mutex_unlock(&all_mutex);
    fclose(tmp);



    //Se eliminaaaaa
    remove(tmpName);
}

typedef struct {
    const char *inFile;
} ThreadArg;

void *threadFunc(void *arg) {
    ThreadArg *t = (ThreadArg*)arg;
    
    printf("[TID %lu] INICIO: %s\n", (unsigned long)pthread_self(), t->inFile);

    extern FILE *f_all;
    compressFileToAll(t->inFile, f_all);

    printf("[TID %lu] FIN   : %s\n", (unsigned long)pthread_self(), t->inFile);
    return NULL;
}

// archiovo globaal para comp_todos

FILE *f_all;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <libro1.txt> [libro2.txt ...]\n", argv[0]);
        return 1;
    }
    mkdir("salida", 0755);
    // ESTO ES NUEVO: IDEA  , NO SE SI FUNCIONAA
    f_all = fopen("salida/comp_todos.bin", "wb");
    if (!f_all) { perror("comp_todos.bin"); return 1; }

    int n = argc - 1;
    pthread_t *threads = malloc(n * sizeof(pthread_t));
    ThreadArg *args   = malloc(n * sizeof(ThreadArg));

    struct timespec t0, t1;


    clock_gettime(CLOCK_MONOTONIC, &t0);



    //FORR
    for (int i = 0; i < n; ++i) {
        args[i].inFile = argv[i+1];
        if (pthread_create(&threads[i], NULL, threadFunc, &args[i]) != 0) {
            perror("Error creando hilo");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n; ++i) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long dur_ns = (t1.tv_sec - t0.tv_sec) * 1000000000L + (t1.tv_nsec - t0.tv_nsec);


//CARRERA
    printf("Tiempo total paralelo: %ld ns\n", dur_ns);




    printf("Archivos procesados: %d\n", n);

    fclose(f_all);  



    free(threads);

    free(args);
    return 0;
}
