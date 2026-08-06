/**
 * @file myshell.c
 * @author adaskin
 * @brief asimple shell program, written in class for educational purposes
 * it may include afew bugs...
 *
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#define INBUF_SIZE 256

int main() {

  char inbuf[INBUF_SIZE] = {'\0'};
  int nbyte; /* input byte count */
  printf("myshell id: %d\n", getpid());
  while (1) {
    write(1, "$", 2);

    if ((nbyte = read(0, inbuf, 255)) <= 0) {
      perror("input <=0 byte");
    } else {
      inbuf[nbyte + 1] = '\0';
    }
    "cat\0 afile bfile cfile";
    // char *command = NULL;
    // char *token = strtok(inbuf, " \t\n");
    // if(token != NULL)  command = token;
    int r = system(inbuf);
  }
}