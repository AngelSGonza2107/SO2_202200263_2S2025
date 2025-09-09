#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define SYS_MOVE_MOUSE 463

int main() {
    int dx = 200;
    int dy = 100;
    long res = syscall(SYS_MOVE_MOUSE, dx, dy);
    if (res == 0)
        printf("Movimiento del mouse simulado: dx=%d, dy=%d\n", dx, dy);
    else
        perror("Error en syscall move_mouse");
    return 0;
}
