
#include <unistd.h>     /* pid_t getpid (void); */
#include <sys/types.h>  /* maybe needed in some UNIX*/
#include <stdio.h>
int main(){

   pid_t pid = getpid();
   printf("process id: %d - parent id: %d\n", pid, getppid());
   return 0;
}

  
