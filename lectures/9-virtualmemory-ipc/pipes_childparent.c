#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
    int fd[2], nbytes;
    pid_t childpid;
    char string[] = "Hello, world!\n";
    char readbuffer[80];

    pipe(fd);
    /*
    file descriptors remain open across child and parent processes.
    If we call fork after creating a pipe,
    then the parent and child can communicate via the pipe.*/

    if ((childpid = fork()) == -1) {
        perror("fork");
        exit(1);
    }

    if (childpid == 0) { /*child*/

        /* close input side of pipe */
        close(fd[0]);

        /* Send "string" through the output side of pipe */
        write(fd[1], string, (strlen(string) + 1));
        exit(0);
    } else { /*parent*/
        /* close output side of pipe */
        close(fd[1]);

        /* Read from the pipe */
        nbytes = read(fd[0], readbuffer, sizeof(readbuffer));
        printf("Received string: %s", readbuffer);
    }

    return (0);
}
