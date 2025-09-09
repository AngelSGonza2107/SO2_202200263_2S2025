#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "screen_capture.h" // Tu cabecera compartida

#define SYS_CAPTURE_SCREEN 466 // El número de tu nueva syscall

int main() {
    // 1. Reservar un buffer grande para la captura (ej. para una pantalla 8K)
    const size_t buffer_size = 7680 * 4320 * 4; 
    unsigned char *pixel_buffer = malloc(buffer_size);
    if (!pixel_buffer) {
        fprintf(stderr, "Error al reservar memoria.\n");
        return 1;
    }

    // 2. Preparar la estructura
    struct screen_capture capture_data;
    capture_data.buffer_size = buffer_size;
    capture_data.buffer = (uint64_t)pixel_buffer;

    // 3. Llamar a la syscall
    printf("Capturando la pantalla...\n");
    long ret = syscall(SYS_CAPTURE_SCREEN, &capture_data);
    
    if (ret != 0) {
        perror("Error en la syscall capture_screen");
        free(pixel_buffer);
        return 1;
    }

    printf("¡Captura exitosa!\n");
    printf("  Resolución: %u x %u\n", capture_data.width, capture_data.height);
    printf("  Bits por píxel: %u\n", capture_data.bpp);

    // Ahora `pixel_buffer` contiene la imagen.
    // Puedes guardarla en un archivo, procesarla, etc.

    free(pixel_buffer);
    return 0;
}