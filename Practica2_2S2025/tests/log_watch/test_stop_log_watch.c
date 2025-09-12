#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>

#define SYS_STOP_LOG_WATCH 470

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Error: %s <ID_MONITOREO>\n", argv[0]);
        return 1;
    }

    uint32_t id = (uint32_t)strtoul(argv[1], NULL, 10);

    if (syscall(SYS_STOP_LOG_WATCH, id) == -1) {
        perror("Error al detener el monitoreo");
        return 1;
    }

    printf("Monitoreo con ID %u detenido exitosamente.\n", id);
    return 0;
}
