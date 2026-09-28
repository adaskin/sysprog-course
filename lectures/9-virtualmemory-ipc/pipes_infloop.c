/*Hereâ€™s a complete example that doesnâ€™t work!
The child reads one byte at a time from the pipe
and prints it out -
but we never see the message!
Can you see why?
The parent sends the bytes H,i,(space),C...! into the pipe.
The child starts reading the pipe one byte at a time.
In the above case, the child process will read and print each character.
However, it never leaves the while loop!
When there are no characters left to read it simply blocks and waits for more
unless All the writers close their ends */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {
    int fd[2];
    pipe(fd);
    // You must read from fd[0] and write from fd[1]
    printf("Reading from %d, writing to %d\n", fd[0], fd[1]);
    pid_t p = fork();
    if (p > 0) {
        close(fd[0]);
        write(fd[1], "Hi Child!", 9);
        close(fd[1]);
        wait(NULL);

    } else {
        char buf;
        int bytesread;
        close(fd[1]);
        // read one byte at a time.
        while ((bytesread = read(fd[0], &buf, 1)) > 0) {
            putchar(buf);
            printf("%d-%c\n", buf, buf);
        }
    }
    return 0;
}