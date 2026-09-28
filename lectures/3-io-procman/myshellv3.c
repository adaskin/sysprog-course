/**
 * @file myshell.c
 * @author adaskin
 * @brief ADDED fork(),
 * asimple shell program,
 * written in class for educational purposes
 * it may include afew bugs...
 *
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INBUF_SIZE 256

int main(int argc, char *argv[]) {

  char inbuf[INBUF_SIZE] = {'\0'};
  int nbyte; /* input byte count */
  printf("myshell id: %d\n", getpid());
  while (1) {
    write(1, "$", 2);

    if ((nbyte = read(0, inbuf, 255)) <= 0) {
      perror("input <=0 byte");
    } else {
      inbuf[nbyte - 1] = '\0';
    }

    printf("inbuf:%.255s", inbuf);
    if (strncmp(inbuf, "exit", 4) == 0) {
      exit(0);
    }
    pid_t child_pid = fork();
    printf("process id: %d - parent id: %d\n", getpid(), getppid());
    /*


      */
    if (child_pid == 0) {
      printf("Child process!!!\n");
      int r = execl(inbuf, inbuf, NULL);

      if (r == -1) {
        char command[255] = {'/', 'b', 'i', 'n', '/', '\0'};
        strncat(command, inbuf, 250);
        r = execl(command, inbuf, NULL);
        if (r == -1)
          perror("execl");
      }
    } else if (child_pid > 0) {
      printf("Parent process Bye Bye!!!\n");
    } else {
      perror("fork() hata");
    }
  }
}