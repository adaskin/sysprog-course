/*written in lecture for educaitonal purposes
*/
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <stdio.h>
int main(){
   
   int len  = 100;
   off_t offset = 0;
   int fd2 = shm_open("hello", O_RDWR);
   char *addr = mmap(NULL, len, PROT_WRITE,
                   MAP_SHARED, fd2, offset);
   int pid = fork();
   if (pid == 0){
    addr[0] = 'C';
    addr[1] = 'C';
    printf("\n\nChild\n%.100s\n", addr);
  
   }else{
     addr[2] = 'P';
     addr[3] = 'P';
     wait(NULL);
     printf("\n\nParent\n%c, %c, %c, %c\n", 
        addr[0], addr[1], addr[2], addr[3]);
  
   }
   munmap(addr, len);
   //close(fd2);
}

