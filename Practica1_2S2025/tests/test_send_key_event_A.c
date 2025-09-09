#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define SYS_SEND_KEY_EVENT 464

#define KEY_A 30
#define KEY_B 48
#define KEY_ENTER 28

int main() {
    int keycode = KEY_A;

    long result = syscall(SYS_SEND_KEY_EVENT, keycode);

    if (result == 0) {
        printf("Syscall send_key_event ejecutada correctamente (keycode = %d)\n", keycode);
    } else {
        perror("Error en syscall send_key_event");
    }

    return 0;
}
