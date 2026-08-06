#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main() {
  int fd = open("newfile", O_CREAT | O_TRUNC | O_WRONLY, 0666);
  int pid = fork();
  if (pid > 0) {
    char *str = "parent \n";
    write(fd, str, strlen(str));
    close(fd);
  } else if (pid == 0) {
    char *str = "child \n";
    write(fd, str, strlen(str));
    close(fd);
  } else {
    perror("fork");
    exit(1);
  }
}
