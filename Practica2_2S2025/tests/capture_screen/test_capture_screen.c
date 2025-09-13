#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <png.h>

#define SYS_CAPTURE_SCREEN 466

// Estructura para pasar datos entre el espacio de usuario y el kernel
struct capture_struct {
    uint64_t data_pointer;
    uint64_t data_size;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_row;
    uint32_t bytes_per_pixel;
    uint32_t total_bytes;
};

// Llama a la syscall definida en el kernel
static long sys_capture_screen(struct capture_struct *c_struct) {
    return syscall(SYS_CAPTURE_SCREEN, c_struct);
}

// Convierte datos de imagen en formato BGR(A) a RGB
static int convert_to_rgb(const uint8_t *src, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp, uint8_t *dst) {
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *src_row = src + (size_t)pitch * y;
        uint8_t *dst_row = dst + (size_t)width * 3 * y;

        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t *p = src_row + x * (bpp / 8);
            uint8_t *q = dst_row + x * 3;

            if (bpp == 32) { q[0] = p[2]; q[1] = p[1]; q[2] = p[0]; }     // BGRA -> RGB
            else if (bpp == 24) { q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; } // RGB
            else return -1;
        }
    }
    return 0;
}

// Guarda datos RGB en un archivo PNG
// Más sencillo con mapa de bits RGB sin compresión
static int save_png(const char *path, const uint8_t *rgb, uint32_t w, uint32_t h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return -1; }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_write_struct(&png_ptr, NULL); fclose(fp); return -1; }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return -1;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    for (uint32_t y = 0; y < h; ++y) {
        png_bytep row = (png_bytep)(rgb + (size_t)w * 3 * y);
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    // Primera llamada para obtener el tamaño necesario
    struct capture_struct c_struct = {0};
    uint8_t dummy = 0;
    c_struct.data_pointer = (uint64_t)(uintptr_t)&dummy;
    c_struct.data_size = 1;

    if (sys_capture_screen(&c_struct) != -1 || errno != ENOSPC) {
        return 1;
    }

    // Extrae los metadatos de la imagen para preparar el buffer que se enviará a la syscall
    uint32_t w = c_struct.width, h = c_struct.height, pitch = c_struct.bytes_per_row, bpp = c_struct.bytes_per_pixel;
    if (w == 0 || h == 0 || pitch == 0 || (bpp != 24 && bpp != 32)) {
        fprintf(stderr, "Metadatos inválidos");
        return 1;
    }

    size_t raw_size = (size_t)pitch * h;
    size_t rgb_size = (size_t)w * h * 3;

    uint8_t *raw = malloc(raw_size);
    uint8_t *rgb = malloc(rgb_size);
    if (!raw || !rgb) {
        perror("malloc");
        free(raw); free(rgb);
        return 1;
    }

    // Segunda llamada para capturar la pantalla
    c_struct.data_pointer = (uint64_t)(uintptr_t)raw;
    c_struct.data_size = raw_size;

    if (sys_capture_screen(&c_struct) != 0) {
        perror("captura_screen");
        free(raw); free(rgb);
        return 1;
    }

    // Convierte de BGR(A) a RGB y guarda la imagen en un archivo PNG
    if (convert_to_rgb(raw, w, h, pitch, bpp, rgb) != 0) {
        free(raw); free(rgb);
        return 1;
    }

    if (save_png(argv[1], rgb, w, h) != 0) {
        free(raw); free(rgb);
        return 1;
    }

    printf("¡Captura guardada exitosamente!\n");
    free(raw);
    free(rgb);
    return 0;
}
