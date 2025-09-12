#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SYS_IPC_CHANNEL_RECEIVE 468
#define MAX_MSG_SIZE 256

int main(void) {
    char data_msg[MAX_MSG_SIZE + 1];
    long bytes_recibidos;

    bytes_recibidos = syscall(SYS_IPC_CHANNEL_RECEIVE, data_msg, MAX_MSG_SIZE);
    
    if (bytes_recibidos < 0) {
        perror("sys_ipc_channel_receive fallo");
        return 1;
    }

    data_msg[bytes_recibidos] = '\0';
    
    printf("Mensaje recibido: \"%s\" | Bytes recibidos: %ld bytes\n", data_msg, bytes_recibidos);

    return 0;
}