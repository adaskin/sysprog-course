#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main() {
    int pipe_fds[2];
    /* Call with the O_CLOEXEC flag to prevent any commands from blocking */
    pipe2(pipe_fds, O_CLOEXEC);
    /*Remember for pipe_fds,
    the program read then write
    (reading is 0 and writing is 1)*/

    if (!fork()) { /* Child */

        /* Make the stdout of the process, the write end */
        dup2(pipe_fds[1], 1);
        /* Exec! Don’t forget the cast*/
        execlp("ls", "ls", "-1", (char *)NULL);
        exit(-1);
    }
    // Same here, except the stdin of the process is the read end
    dup2(pipe_fds[0], 0);
    // Same deal here
    execlp("cut", "cut", "-f1", "-d.", (char *)NULL);
    exit(-1);
    return 0;
}