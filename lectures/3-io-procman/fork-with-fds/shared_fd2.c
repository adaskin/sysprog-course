/**
 * shared fd: 1 process reads, the other writes
 *
 * */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int fd = open("newfile",
                O_CREAT | O_TRUNC | O_RDWR,
                S_IRUSR | S_IWUSR);
  pid_t pid = fork();
  if (pid > 0) {
    char buf[100];
    int nbytes = 0;
    int stat = 0;

    /*wait for child to terminate*/
    waitpid(pid, &stat, 0);

    /*read file from the begginning*/
    lseek(fd, 0, SEEK_SET);
    do {
      nbytes = read(fd, buf, 99);
      if (nbytes == -1) {
        perror("read error:");
        break;
      }
      buf[nbytes] = '\0';
      // write(1, buf, nbytes);
      printf("size:%d-msg:%s", nbytes, buf);
    } while (nbytes > 0);
    close(fd);
  } else if (pid == 0) {
    char buf[256] = "\0";
    sprintf(buf, "id: %d-msg:%.100s",
           getpid(), "hi from child\n");

    /*writes to shared file*/
    write(fd, buf, strlen(buf));
    close(fd);
  } else {
    perror("fork");
    exit(1);
  }
}