#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define SYS_GET_SCREEN_RESOLUTION 550

int main() {
    int width = 0, height = 0;

    long ret = syscall(SYS_GET_SCREEN_RESOLUTION, &width, &height);
    if (ret == 0) {
        printf("Screen resolution: %dx%d\n", width, height);
    } else {
        perror("syscall get_screen_resolution");
        printf("Return value: %ld, errno: %d\n", ret, errno);
    }

    return 0;
}