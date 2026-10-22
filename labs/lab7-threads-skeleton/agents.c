/* agents.c — Task 1b and 1c (YOUR CODE).
 *
 *   ./agents            collect results with pthread_join, print the total
 *   ./agents --exit     collect results returned through pthread_exit / pthread_join
 *   ./agents --detach   detach the agents (1c: explain what main must do before it exits)
 *
 * The total printed must be the same on every run for the same seeds.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#define N_AGENTS 8
#define STEPS    100000

typedef struct {
    int      id;
    unsigned seed;      /* per-agent seed for rand_r */
    long     steps;     /* how much work to do        */
    long     result;    /* filled in by the agent     */
} agent_t;

/* TODO(1b): do `steps` iterations of `result += rand_r(&seed) % 100`, store the result in the
 * agent's own struct, and return NULL. Never touch a shared variable. */
static void *agent_work(void *arg) {
    agent_t *a = arg;
    (void)a;
    return NULL;
}

/* TODO(1c): same work, but allocate the result on the heap and return it
 * (`pthread_exit` or `return`); main frees it after pthread_join. */
static void *agent_work_returning(void *arg) {
    (void)arg;
    return NULL;
}

int main(int argc, char **argv) {
    int use_exit   = argc > 1 && strcmp(argv[1], "--exit") == 0;
    int use_detach = argc > 1 && strcmp(argv[1], "--detach") == 0;
    (void)use_exit;
    (void)use_detach;
    (void)agent_work;               /* remove these two lines once you use the functions */
    (void)agent_work_returning;

    /* TODO(1b): create N_AGENTS threads, each with its OWN agent_t (id = i, seed = 1000 + i).
     * Join them all, sum the results, and print: "total = <n>". */

    /* TODO(1c): the --exit and --detach variants. */

    return 0;
}
