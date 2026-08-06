/*all processes write to pipe, but none reads from it.
in this case, SIGPIPE is generated.
we handle this in signal handler.
*/
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/*signal handler*/
void no_one_listening(int signal) {
    write(1, "No one is listening!\n", 21);
}
int main() {
    signal(SIGABRT, no_one_listening);
    abort();

    int filedes[2];
    pipe(filedes);
    pid_t child = fork();
    if (child > 0) { /*  the parent.*/
                     /* Close the listening end of the pipe */
        close(filedes[0]);
        wait(NULL);
    } else {
        /* Child writes messages to the pipe */
        write(filedes[1], "One", 3);
        close(filedes[0]);
        sleep(2);
        write(filedes[1], "Two", 3);
        write(1, "Done\n", 5);
    }
    return 0;
}