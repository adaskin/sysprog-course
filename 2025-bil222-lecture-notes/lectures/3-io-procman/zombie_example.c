/*zombie process example

*/
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <sys/wait.h>
int main() {
   pid_t cpid = fork();
   if(cpid > 0){
       printf("parent id: %d, child id = %d.\n", getpid(),  cpid);
       /*in terminal $ps aux |grep "defunct"
       see the ZOMBIE process*/
       getchar();/*wait for user input*/
       
       pid_t w = wait(NULL);
       if (w == -1){
           perror("wait:"); exit(EXIT_FAILURE);   
       }else{
           printf("parent id: %d, ZOMBIE child id = %d.\n",
               getpid(), w);
          exit(EXIT_SUCCESS);
       }
   }else if(cpid == -1){
       perror("fork:");
       exit(EXIT_FAILURE);
   }
}


