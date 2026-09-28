#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
int main(){
 
    char buf[100];
    int cpid = fork();
    if (cpid == 0){
        int fd = open("file.txt", O_RDONLY);
        if(read(fd, buf, 10) == -1) perror("child:read");
        buf[10] = '\0';
        printf("child: %s\n", buf);
        close(fd);
    }else if(cpid >0){
        int fd = open("file.txt", O_RDONLY);
        wait(0);
        if(read(fd, buf, 10) == -1) perror("child:read");
        buf[10] = '\0';
        printf("parent: %s\n", buf);
        close(fd);
    }else perror("fork:");
}