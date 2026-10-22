/* race.c — Task 3 (given). It has TWO races ON PURPOSE. Do not fix this file:
 * copy your fix into race_fixed.c.
 *
 *   ./race <nthreads> <steps_per_thread>
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define MAX_STEPS 1000000L
#define MAX_RACE_THREADS 16

long sum = 0;                              /* shared by every thread  */
long results[MAX_RACE_THREADS * MAX_STEPS];      /* shared by every thread  */
int  n_done = 0;                           /* shared by every thread  */

static void *agent(void *arg) {
    long steps = *(long *)arg;
    for (long i = 0; i < steps; i++) {
        sum += 1;                          /* bug 1: lost updates              */
        results[n_done] = i;               /* bug 2: two agents, same slot ... */
        n_done++;                          /* ... and lost updates here too    */
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <nthreads> <steps_per_thread>\n", argv[0]);
        return 1;
    }
    int  nthreads = atoi(argv[1]);
    long steps    = atol(argv[2]);
    if (nthreads < 1 || nthreads > MAX_RACE_THREADS || steps < 1 || steps > MAX_STEPS) {
        fprintf(stderr, "nthreads must be 1..%d, steps 1..%ld\n", MAX_RACE_THREADS, MAX_STEPS);
        return 1;
    }

    pthread_t tid[MAX_AGENTS];
    for (int t = 0; t < nthreads; t++) {
        CHECK(pthread_create(&tid[t], NULL, agent, &steps));
    }
    for (int t = 0; t < nthreads; t++) {
        CHECK(pthread_join(tid[t], NULL));
    }

    long expected = (long)nthreads * steps;
    printf("sum    = %ld (expected %ld)\n", sum, expected);
    printf("n_done = %d (expected %ld)\n", n_done, expected);
    return 0;
}
