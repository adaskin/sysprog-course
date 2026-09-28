#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
void sig_handler(int sig){
    printf("sig: %d", sig);
    
    //while(1);
    //exit(0);
}
int main(){
   printf("merhaba");
   signal(SIGINT, sig_handler);
   while(1);
}