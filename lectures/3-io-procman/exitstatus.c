#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  pid_t cpid = fork();
  if (!cpid) {
    execl("./program2", "program2", NULL);
  } else {
    for (int i = 0; i < 100; i++) {
      printf("program 1\n"); // sleep(1);
    }
    int status, w;

    do {
      w = waitpid(cpid, &status, WNOHANG);
      if (w > 0) {
        if (WIFEXITED(status))
          printf("w: %d, program exit val: %d\n", w, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
          printf("signalled: %d", WTERMSIG(status));
      }
      printf("child is still working\n");
      sleep(1);

    } while (w == 0);
  }
  return 0;
}
