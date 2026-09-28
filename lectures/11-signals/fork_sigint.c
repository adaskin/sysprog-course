#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#define N 5
int main()
{
    pid_t pid[N];
    int i, child_status;
    //signal(SIGINT, SIG_IGN);
    for (i = 0; i < N; i++)
        if ((pid[i] = fork()) == 0)
        {
           
           while (1)
               ; /* Child infinite loop */
        }
    /* Parent terminates the child processes */
    for (i = 0; i < N; i++)
    {
        printf("Killing process %d\n", pid[i]);
        kill(pid[i], SIGINT);
    }
    /* Parent reaps terminated children */
    for (i = 0; i < N; i++)
    {
        pid_t wpid = wait(&child_status);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit status %d\n", wpid, WEXITSTATUS(child_status));
        else if(WIFSIGNALED(child_status))
            printf("Child %d terminated with signal %d\n", wpid, WTERMSIG(child_status));
    }
}

