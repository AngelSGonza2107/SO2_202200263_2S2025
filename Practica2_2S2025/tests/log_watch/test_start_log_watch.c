#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>

#define SYS_START_LOG_WATCH 469

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Error, se espera: %s <log_central> <palabra_clave> <log1> [log2 ... logN]\n", argv[0]);
        return 1;
    }

    const char *central_log = argv[1];
    const char *keyword = argv[2];
    const char *const *log_paths = (const char *const *)&argv[3];

    long id = syscall(SYS_START_LOG_WATCH, log_paths, central_log, keyword);
    if (id == -1) {
        perror("Error al iniciar monitoreo");
        return 1;
    }

    printf("ID Monitoreo = %ld\n", id);
    return 0;
}
