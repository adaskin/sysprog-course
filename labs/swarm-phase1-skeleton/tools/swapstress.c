/* swapstress.c — Task 4: provokes the grid_swap deadlock (given).
 *
 *   ./swapstress <row|global> <threads> <seconds>
 *
 * Every thread repeatedly swaps two vertically adjacent cells of an 8x8 grid, in a random
 * orientation (sometimes the upper cell first, sometimes the lower one first).
 * Prints "ok: N swaps" or "HANG: possible deadlock" (exit status 3).
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "grid.h"

static grid_t *g;
static volatile int stop;
static volatile int finished;
static long swaps_per_thread[64];

static void *worker(void *arg) {
    long id = (long)arg;
    unsigned seed = (unsigned)id * 2654435761u + 1;
    long n = 0;
    while (!stop) {
        int x = rand_r(&seed) % 8;
        int y = rand_r(&seed) % 7;                     /* swap (x, y) and (x, y + 1) */
        if (rand_r(&seed) & 1) {
            grid_swap(g, x, y, x, y + 1);              /* upper cell first */
        } else {
            grid_swap(g, x, y + 1, x, y);              /* lower cell first */
        }
        n++;
    }
    swaps_per_thread[id] = n;
    __sync_fetch_and_add(&finished, 1);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <row|global> <threads> <seconds>\n", argv[0]);
        return 1;
    }
    int per_row = strcmp(argv[1], "row") == 0;
    int threads = atoi(argv[2]);
    int seconds = atoi(argv[3]);
    if (threads < 2 || threads > 64 || seconds < 1) {
        fprintf(stderr, "threads must be 2..64, seconds >= 1\n");
        return 1;
    }
    g = grid_create(8, 8, per_row);
    if (!g) {
        fprintf(stderr, "grid_create failed (implement grid.c first)\n");
        return 1;
    }
    for (int i = 0; i < 64; i++) {
        grid_claim(g, i % 8, i / 8, i);
    }
    pthread_t t[64];
    for (long i = 0; i < threads; i++) {
        pthread_create(&t[i], NULL, worker, (void *)i);
    }
    sleep((unsigned)seconds);
    stop = 1;
    for (int waited = 0; finished < threads && waited < 30; waited++) {
        usleep(100000);                                /* give them 3 seconds to notice `stop` */
    }
    if (finished < threads) {
        printf("HANG: possible deadlock (%d of %d threads never finished)\n", threads - finished,
               threads);
        return 3;
    }
    long total = 0;
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
        total += swaps_per_thread[i];
    }
    printf("ok: %ld swaps\n", total);
    return 0;
}
