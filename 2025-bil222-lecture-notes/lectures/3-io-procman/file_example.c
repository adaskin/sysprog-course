#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
int main(){
    int fd = open("file.txt", O_RDONLY);
    char buf[100];
    int cpid = fork();/*child1 created*/

    if (cpid == 0){
        int r;
        if((r = read(fd, buf, 50)) == -1) perror("child:read");
        buf[r] = '\0';
        printf("child: %s\n", buf); /*LINE A*/
    }else if(cpid >0){
        int r;
        wait(0);
        if((r = read(fd, buf, 50)) == -1) perror("parent:read");
        buf[r] = '\0';
        printf("parent: %s\n", buf); /*LINE B*/

    }else perror("fork:");
}