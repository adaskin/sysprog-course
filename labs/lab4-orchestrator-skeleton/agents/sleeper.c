#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int seconds = 3;

    if (argc > 1) {
        seconds = atoi(argv[1]);
    }

    printf("Sleeping for %d seconds\n", seconds);
    fflush(stdout);          /* logs are files: stdout is block-buffered! */
    sleep(seconds);
    printf("Sleeper finished\n");

    return 0;
}
