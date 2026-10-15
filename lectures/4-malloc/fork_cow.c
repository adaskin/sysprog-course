#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int x = 10;
    pid_t pid = fork();
    if (pid == 0) {                 /* child */
         printf("child:  x = %d at %p\n", x, (void *)&x);
        x = 42;
        printf("child:  x = %d at %p\n", x, (void *)&x);
        _exit(0);
    }
    wait(NULL);
    printf("parent: x = %d at %p\n", x, (void *)&x);
    return 0;
}