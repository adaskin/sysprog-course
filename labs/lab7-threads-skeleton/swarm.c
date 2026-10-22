/* swarm.c — OPTIONAL BONUS: a swarm without locks (YOUR CODE).
 *
 * 12 performer threads walk randomly on a 20 x 10 grid for 200 steps each, 1 ms per step.
 * Count a COLLISION whenever a performer steps into a cell where another performer already is.
 * No locks. The main thread draws the grid as ASCII every 50 ms.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

#define W 20
#define H 10
#define N_PERFORMERS 12
#define STEPS 200

static int grid[H][W];                /* 0 = empty, otherwise performer id + 1 */
static long collisions = 0;           /* shared on purpose; do not "fix" with a lock */

typedef struct {
    int      id;
    unsigned seed;
    int      x, y;
} performer_t;

/* TODO: for STEPS steps: pick a direction with rand_r, stay inside the grid, check whether the
 * target cell is occupied (collision!), leave the old cell, enter the new one, usleep(1000). */
static void *performer(void *arg) {
    performer_t *p = arg;
    (void)p;
    return NULL;
}

/* TODO: print the grid, one character per cell ('.' empty, a letter for a performer). */
static void draw(void) {
}

int main(void) {
    /* TODO: place the performers on distinct cells, start the threads, draw every 50 ms until they
     * finish, join them, and print the number of collisions. */
    (void)performer;
    (void)draw;
    return 0;
}
