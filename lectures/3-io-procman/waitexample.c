#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFSIZE 100
int main(int argc, char *argv[])
{
    char buf[250] = "/usr/bin/";
    int len = strlen(buf);

    // while (1)
    {

        printf(">> ");
        scanf("%100s", &buf[len]);
        if (strncmp(buf, "quit", 4) == 0)
        {
            printf("bye bye\n");
            return 0;
        }
        printf(">> he hee\n");
        pid_t cpid = fork();

        pid_t myid = getpid();
        pid_t myparent = getppid();
        
        if (cpid == 0)
        {
            
            printf("mydi: %d, child process hey!!!\n",
                   getpid());
            int ret = -1;
            ret = execl(buf, buf, NULL);
            if (ret == -1)
            {
                perror("execl");
                //while (1); //termination signal icin konuldu
                exit(2);
            }
        }
        else if (cpid > 0)
        {
            printf("myid: %d parent process hey!!!\n",
                   getpid());

            int status;
            { // wait(&status);
                int w = waitpid(cpid, &status, WNOHANG);
                if (w == 0)
                {
                    if (WIFEXITED(status))
                    {
                        printf("child process exited with %d\n",
                               WEXITSTATUS(status));
                    }
                    else if (WIFSIGNALED(status))
                    {
                        printf("child process is signaled with %d\n",
                               WTERMSIG(status));
                    }

                    printf("bye bye\n");
                    return 0;
                }
                else
                {
                    sleep(10);
                }
            }while (1);
        }
    }

    return 0;
}
