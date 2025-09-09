#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Se definen las dimensiones de la matriz
#define FILAS 20000
#define COLUMNAS 20000

int main() {
    // Se utiliza el tipo numérico long long para la suma para evitar desbordamiento
    long long suma_total = 0;
    
    // Se asigna memoria dinámicamente para la matriz
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
            // Liberar memoria asignada hasta el momento en caso de error
            for (int j = 0; j < i; j++) {
                free(matriz[j]);
            }
            free(matriz);
            return 1;
        }
    }

    // Inicializar la semilla para los numeros aleatorios
    srand(time(NULL));

    // Se llena la matriz con numeros aleatorios entre 0 y 9 (4 bytes)
    printf("Llenando la matriz con numeros aleatorios...\n");
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLUMNAS; j++) {
            matriz[i][j] = rand() % 10;
        }
    }

    // Se realiza la suma de todos los elementos
    printf("Sumando todos los elementos de la matriz...\n");

    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLUMNAS; j++) {
            suma_total += matriz[i][j];
        }
    }

    printf("\n--- Resultados (Secuencial) ---\n");
    printf("La suma total de la matriz es: %lld\n", suma_total);

    // Se libera la memoria asignada
    printf("\nLiberando memoria...\n");
    for (int i = 0; i < FILAS; i++) {
        free(matriz[i]);
    }
    free(matriz);

    return 0;
}