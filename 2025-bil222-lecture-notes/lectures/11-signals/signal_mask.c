#include <stdio.h> #include <stdlib.h>
#include <signal.h> #include <unistd.h>
int main(void){
   sigset_t set, oset, pset;
   sigemptyset(&set); sigaddset(&set, SIGINT);

   sigprocmask(SIG_BLOCK, &set, &oset);
   printf("Old set was %8.8ld.\n", oset);
   printf("id: %d\n", getpid())

   while(1);
   //kill(getpid(), SIGINT);
   printf("bye bye\n");
   sigprocmask(SIG_UNBLOCK, &set, &oset);
   /* The program terminates with a SIGINT */
   return (0);
}
