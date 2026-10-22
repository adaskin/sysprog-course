/* race_fixed.c — Task 3, part 4 (YOUR CODE). Same command line and output format as race.c:
 *
 *   ./race_fixed <nthreads> <steps_per_thread>
 *
 * Fix BOTH bugs WITHOUT any synchronization primitive: give every thread its own counter and its
 * own range of the results array (thread t writes only
 * results[t*steps .. (t+1)*steps - 1]). After joining, main computes the totals.
 *
 * It must print exactly:
 *   sum    = <n> (expected <n>)
 *   n_done = <n> (expected <n>)
 * with both values equal to nthreads * steps on every run.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define MAX_STEPS 1000000L
#define MAX_RACE_THREADS 16

static long results[MAX_RACE_THREADS * MAX_STEPS];

/* TODO: a per-thread argument struct (thread index, steps, and its own partial counter). */

/* TODO: the thread function. */

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
    (void)results;

    long sum = 0;
    long n_done = 0;
    /* TODO: create the threads, join them, and combine their partial counters into sum and n_done. */

    long expected = (long)nthreads * steps;
    printf("sum    = %ld (expected %ld)\n", sum, expected);
    printf("n_done = %ld (expected %ld)\n", n_done, expected);
    return 0;
}
