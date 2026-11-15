/* compat.h — portability shim (given): macOS has no pthread_barrier_t. */
#ifndef COMPAT_H
#define COMPAT_H

#define _GNU_SOURCE
#include <pthread.h>

#ifdef __APPLE__
#include <errno.h>
typedef struct {
    pthread_mutex_t m;
    pthread_cond_t  c;
    unsigned        count, waiting, generation;
} compat_barrier_t;
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)
static inline int compat_barrier_init(compat_barrier_t *b, unsigned n) {
    pthread_mutex_init(&b->m, NULL); pthread_cond_init(&b->c, NULL);
    b->count = n; b->waiting = 0; b->generation = 0; return 0;
}
static inline int compat_barrier_wait(compat_barrier_t *b) {
    pthread_mutex_lock(&b->m);
    unsigned gen = b->generation;
    if (++b->waiting == b->count) {
        b->generation++; b->waiting = 0;
        pthread_cond_broadcast(&b->c);
        pthread_mutex_unlock(&b->m);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (gen == b->generation) pthread_cond_wait(&b->c, &b->m);
    pthread_mutex_unlock(&b->m);
    return 0;
}
static inline int compat_barrier_destroy(compat_barrier_t *b) {
    pthread_mutex_destroy(&b->m); pthread_cond_destroy(&b->c); return 0;
}
#else
typedef pthread_barrier_t compat_barrier_t;
#define compat_barrier_init(b, n) pthread_barrier_init((b), NULL, (n))
#define compat_barrier_wait(b)    pthread_barrier_wait(b)
#define compat_barrier_destroy(b) pthread_barrier_destroy(b)
#endif

#endif
