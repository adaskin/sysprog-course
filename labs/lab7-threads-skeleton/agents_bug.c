/* agents_bug.c — Task 1a (given). It contains the classic bug ON PURPOSE.
 * Run it ten times, copy the output into your report, and explain it. Do not fix this file;
 * your fix goes in agents.c. */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"

#define N_AGENTS 8

void *agent(void *arg) {
    int id = *(int *)arg;           /* reads the pointed-to int LATER, when the thread runs */
    usleep(1000);
    printf("agent %d reporting\n", id);
    return NULL;
}

int main(void) {
    pthread_t tid[N_AGENTS];
    for (int i = 0; i < N_AGENTS; i++) {
        CHECK(pthread_create(&tid[i], NULL, agent, &i));    /* <-- the bug is in this line */
    }
    for (int i = 0; i < N_AGENTS; i++) {
        CHECK(pthread_join(tid[i], NULL));
    }
    return 0;
}
