#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
int main(){
   pid_t pid;
   pid = fork ();
   if (pid == 0){
       abort();
       printf("division %d", 1);
       return 1; //exit(1);
   }else{
       int status;
       wait(&status);
       if(WIFSIGNALED(status)) printf("exit val: %d\n", WTERMSIG(status));
       
   }
   return 0;
}
