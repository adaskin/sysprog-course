/*program2*/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
int main() {
   printf("program2\n");
   char *args[]={"./program1",NULL};
   int ret = execv(args[0],args);

   /* the rest is not executed!!!*/
  
   if (ret == -1) perror("cannot execute: ");
   printf("end of program2-----");
   return 0;
}



