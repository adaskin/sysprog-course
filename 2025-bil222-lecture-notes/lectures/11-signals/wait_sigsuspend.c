#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

volatile sig_atomic_t pid;
void sigchld_handler(int s)
{
    int olderrno = errno;
    pid = waitpid(-1, NULL, 0); /* Main waiting for nonzero pid */
    errno = olderrno;
}
void sigint_handler(int s)
{
}
int main(int argc, char **argv)
{
    sigset_t mask, prev;
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask, &prev); /* Block SIGCHLD */
    if (fork() == 0)                      /* Child */
        exit(0);
    /* Wait for SIGCHLD to be received */
    pid = 0;
    while (!pid)
        sigsuspend(&prev);
    /* Optionally unblock SIGCHLD */
    sigprocmask(SIG_SETMASK, &prev, NULL);
    /* Do some work after receiving SIGCHLD */ 
    printf(".");

    exit(0);
}