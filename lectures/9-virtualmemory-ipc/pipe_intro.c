#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
    int fd[2];
    int nbytes;
    char string[] = "Hello, world!\n";

    char readbuffer[80];

    pipe(fd);  // [.......]->pipe
    pid_t pid = fork();
    if (pid > 0) {  // parent process

        write(fd[1], string, (strlen(string) + 1));
    } else if (pid == 0) {
        /* Read from the pipe */
        nbytes = read(fd[0], readbuffer, sizeof(readbuffer));
        printf("pid:%d, Received string: %s\n", getpid(), readbuffer);
    }

    close(fd[1]);
    close(fd[0]);

    return (0);
}
