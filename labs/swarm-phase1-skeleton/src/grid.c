/* grid.c — the occupancy grid of one leader (Task 3 and Task 4: YOUR CODE).
 *
 * owner[y * w + x] is the id of the follower in that cell, or -1. Two modes (grid_create):
 *   per_row_locks == 0: one mutex protects the whole grid
 *   per_row_locks != 0: one mutex per row; an operation on (x, y) locks only row y
 */
#include "grid.h"
#include <pthread.h>
#include <stdlib.h>

struct grid {
    int              w, h;
    int              per_row;
    int             *owner;
    pthread_mutex_t *locks;          /* 1 mutex, or h mutexes */
};

grid_t *grid_create(int w, int h, int per_row_locks) {
    if (w < 1 || h < 1) {
        return NULL;
    }
    grid_t *g = calloc(1, sizeof(*g));
    if (!g) {
        return NULL;
    }
    g->w = w;
    g->h = h;
    g->per_row = per_row_locks != 0;
    g->owner = malloc((size_t)w * (size_t)h * sizeof(int));
    int nlocks = g->per_row ? h : 1;
    g->locks = malloc((size_t)nlocks * sizeof(pthread_mutex_t));
    if (!g->owner || !g->locks) {
        free(g->owner);
        free(g->locks);
        free(g);
        return NULL;
    }
    for (int i = 0; i < w * h; i++) {
        g->owner[i] = -1;
    }
    for (int i = 0; i < nlocks; i++) {
        pthread_mutex_init(&g->locks[i], NULL);
    }
    return g;
}

void grid_destroy(grid_t *g) {
    if (!g) {
        return;
    }
    int nlocks = g->per_row ? g->h : 1;
    for (int i = 0; i < nlocks; i++) {
        pthread_mutex_destroy(&g->locks[i]);
    }
    free(g->owner);
    free(g->locks);
    free(g);
}

/* The mutex that protects row y (the same mutex for every row in one-lock mode). */
static pthread_mutex_t *lock_for_row(grid_t *g, int y) {
    return g->per_row ? &g->locks[y] : &g->locks[0];
}

static int inside(grid_t *g, int x, int y) {
    return x >= 0 && x < g->w && y >= 0 && y < g->h;
}

int grid_claim(grid_t *g, int x, int y, int id) {
    /* TODO: outside -> -1. Lock lock_for_row(g, y); if the cell is empty, store id and return 0,
     * otherwise return -1. Unlock on every path. */
    (void)g;
    (void)x;
    (void)y;
    (void)id;
    (void)inside;
    (void)lock_for_row;
    return -1;
}

int grid_release(grid_t *g, int x, int y, int id) {
    /* TODO: outside -> -1. Lock; if the cell holds id, make it empty and return 0, else -1. */
    (void)g;
    (void)x;
    (void)y;
    (void)id;
    return -1;
}

int grid_owner(grid_t *g, int x, int y) {
    /* TODO: outside -> -1. Lock, read, unlock. (A reader must lock too: why?) */
    (void)g;
    (void)x;
    (void)y;
    return -1;
}

/* KNOWN BUG (Task 4): in per-row mode this can deadlock. Reproduce it with tools/swapstress,
 * then fix it by choosing a lock ORDER that every caller follows. */
int grid_swap(grid_t *g, int x1, int y1, int x2, int y2) {
    if (!inside(g, x1, y1) || !inside(g, x2, y2)) {
        return -1;
    }
    pthread_mutex_t *a = lock_for_row(g, y1);
    pthread_mutex_t *b = lock_for_row(g, y2);
    pthread_mutex_lock(a);
    if (b != a) {
        pthread_mutex_lock(b);
    }
    int tmp = g->owner[y1 * g->w + x1];
    g->owner[y1 * g->w + x1] = g->owner[y2 * g->w + x2];
    g->owner[y2 * g->w + x2] = tmp;
    if (b != a) {
        pthread_mutex_unlock(b);
    }
    pthread_mutex_unlock(a);
    return 0;
}
