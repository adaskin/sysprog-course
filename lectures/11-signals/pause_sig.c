#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
void sigint_handler(int sig) /* SIGINT handler */{
    /*Note: printf is not async-signal safe (shouldn't be used here)*/
    printf("So you think you can stop the bomb with ctrl-c, do you?\n");
    sleep(2);
    printf("Well...");
    fflush(stdout);
    
    sleep(3);
    printf("OK. :-)\n");
    exit(0);
}

int main()
{

    /* Install the SIGINT handler */
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        perror("signal error");

    /* Wait for the receipt of a signal */
    printf("before pause\n");
    pause();
    printf("after pause\n");

    return 0;
}