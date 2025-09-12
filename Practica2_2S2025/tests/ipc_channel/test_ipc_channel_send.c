#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SYS_IPC_CHANNEL_SEND 467

int main(int argc, char **argv) {
    const char *msg;
    long bytes_enviados;

    if (argc < 2) {
        fprintf(stderr, "Error: Se necesita al menos un mensaje como argumento.\n");
        return 1;
    }

    msg = argv[1];
    
    bytes_enviados = syscall(SYS_IPC_CHANNEL_SEND, msg, strlen(msg));
    
    if (bytes_enviados < 0) {
        perror("sys_ipc_channel_send fallo");
        return 1;
    }
    
    printf("Mensaje enviado: \"%s\" | Bytes enviados: %ld bytes\n", msg, bytes_enviados);
    
    return 0;
}