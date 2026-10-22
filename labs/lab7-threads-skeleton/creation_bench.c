/* creation_bench.c — Task 2, "Processes vs. threads" (YOUR CODE).
 *
 * Time creating and joining 10,000 threads that return immediately, and creating and reaping
 * 10,000 processes (fork + _exit + waitpid). Print both times and the cost per creation.
 *
 * Remember (Lab 4 / Week 4): the child of fork must call _exit, not exit.
 */
#include <pthread.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"

#define N 10000

static void *nothing(void *arg) {
    return arg;
}

int main(void) {
    /* TODO: time N create+join cycles of `nothing`. */
    double thread_secs = 0;

    /* TODO: time N fork+_exit+waitpid cycles. */
    double process_secs = 0;

    printf("threads:   %.3f s total, %.2f us each\n", thread_secs, thread_secs / N * 1e6);
    printf("processes: %.3f s total, %.2f us each\n", process_secs, process_secs / N * 1e6);
    (void)nothing;
    return 0;
}
