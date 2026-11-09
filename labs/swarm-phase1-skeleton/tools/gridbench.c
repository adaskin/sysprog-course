/* gridbench.c — Task 3: how much does lock granularity matter? (given)
 *
 *   ./gridbench <row|global> <threads> <seconds>
 *
 * Every thread repeatedly claims a random cell, reads it, and releases it, on a 64x36 grid, and
 * the program prints the total number of operations per second. Compare `global` with `row` for
 * 1, 2, 4, 8 threads (and look at how the numbers scale).
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "grid.h"

#define W 64
#define H 36

static grid_t *g;
static volatile int stop;

static void *worker(void *arg) {
    long id = (long)arg;
    unsigned seed = (unsigned)id * 2654435761u + 12345u;
    long ops = 0;
    while (!stop) {
        int x = (int)(rand_r(&seed) % W), y = (int)(rand_r(&seed) % H);
        if (grid_claim(g, x, y, (int)id) == 0) {
            if (grid_owner(g, x, y) == (int)id) {
                ops++;
            }
            grid_release(g, x, y, (int)id);
        }
        ops += 2;
    }
    return (void *)ops;
}

int main(int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[1], "row") && strcmp(argv[1], "global"))) {
        fprintf(stderr, "usage: %s <row|global> <threads> <seconds>\n", argv[0]);
        return 2;
    }
    int nt = atoi(argv[2]), secs = atoi(argv[3]);
    if (nt < 1 || nt > 64 || secs < 1) {
        return 2;
    }
    g = grid_create(W, H, strcmp(argv[1], "row") == 0);
    if (!g) {
        fprintf(stderr, "grid_create failed\n");
        return 1;
    }
    pthread_t th[64];
    for (long i = 0; i < nt; i++) {
        pthread_create(&th[i], NULL, worker, (void *)i);
    }
    sleep((unsigned)secs);
    stop = 1;
    long total = 0;
    for (int i = 0; i < nt; i++) {
        void *r;
        pthread_join(th[i], &r);
        total += (long)r;
    }
    printf("%-6s threads=%-2d  %.2f million ops/s\n", argv[1], nt, (double)total / secs / 1e6);
    grid_destroy(g);
    return 0;
}
