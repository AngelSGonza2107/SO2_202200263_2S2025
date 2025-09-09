#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// Se definen las dimensiones de la matriz y el número de hilos a utilizar
#define FILAS 20000
#define COLUMNAS 20000
// En mi caso utilicé la cantidad de procesadores lógicos de mi CPU
#define NUM_HILOS 8

// Estructura para pasar datos a cada hilo
typedef struct {
    int id_hilo;
    int **matriz;
    int fila_inicio;
    int fila_fin;
    long long suma_parcial;
} DatosHilos;

// Función de suma que ejecutará cada hilo
void* sumar_seccion(void* arg) {
    DatosHilos* datos = (DatosHilos*) arg;
    datos->suma_parcial = 0;

    // Cada hilo suma su sección asignada de la matriz
    for (int i = datos->fila_inicio; i < datos->fila_fin; i++) {
        for (int j = 0; j < COLUMNAS; j++) {
            datos->suma_parcial += datos->matriz[i][j];
        }
    }
    
    pthread_exit(NULL);
}

int main() {
    // Se asigna memoria para la matriz
    printf("Asignando memoria para la matriz de %d x %d...\n", FILAS, COLUMNAS);
    int **matriz = (int **)malloc(FILAS * sizeof(int *));
    if (matriz == NULL) {
        fprintf(stderr, "Error al asignar memoria para las filas.\n");
        return 1;
    }
    for (int i = 0; i < FILAS; i++) {
        matriz[i] = (int *)malloc(COLUMNAS * sizeof(int));
        if (matriz[i] == NULL) {
            fprintf(stderr, "Error al asignar memoria para la columna %d.\n", i);
            return 1;
        }
    }
    
    // Se llena la matriz con números aleatorios
    srand(time(NULL));
    printf("Llenando la matriz con numeros aleatorios...\n");
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLUMNAS; j++) {
            matriz[i][j] = rand() % 10;
        }
    }

    // Iniciar la suma con el multithreading
    printf("Sumando todos los elementos de la matriz con %d hilos...\n", NUM_HILOS);
    pthread_t hilos[NUM_HILOS];
    DatosHilos datos_hilos[NUM_HILOS];
    int filas_por_hilo = FILAS / NUM_HILOS;

    // Crear y lanzar los hilos para que realicen las tareas
    for (int i = 0; i < NUM_HILOS; i++) {
        datos_hilos[i].id_hilo = i;
        datos_hilos[i].matriz = matriz;
        datos_hilos[i].fila_inicio = i * filas_por_hilo;
        datos_hilos[i].fila_fin = (i == NUM_HILOS - 1) ? FILAS : (i + 1) * filas_por_hilo;
        
        pthread_create(&hilos[i], NULL, sumar_seccion, (void*)&datos_hilos[i]);
    }
    
    // Esperar a que todos los hilos terminen
    for (int i = 0; i < NUM_HILOS; i++) {
        pthread_join(hilos[i], NULL);
    }
    
    // Sumar los resultados parciales de cada hilo (utilizando long long para evitar desbordamiento)
    long long suma_total = 0;
    for (int i = 0; i < NUM_HILOS; i++) {
        suma_total += datos_hilos[i].suma_parcial;
    }

    printf("\n--- Resultados (Multithreading) ---\n");
    printf("La suma total de la matriz es: %lld\n", suma_total);

    // Liberar memoria
    printf("\nLiberando memoria...\n");
    for (int i = 0; i < FILAS; i++) {
        free(matriz[i]);
    }
    free(matriz);
    
    return 0;
}