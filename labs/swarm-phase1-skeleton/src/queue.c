/* queue.c — bounded thread-safe queue (Task 1: YOUR CODE).
 *
 * A ring buffer of `capacity` items of `item_size` bytes, copied by value.
 * Rules (the tests check them):
 *   - `while`, not `if`, around every pthread_cond_wait
 *   - queue_pop blocks while empty; queue_push blocks while full
 *   - after queue_close: push and try_push fail with -1 (also for a pusher that was already
 *     blocked), pop keeps returning the remaining items and then fails with -1
 *   - no busy-waiting, no sleeping for synchronization
 */
#include "queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct queue {
    char           *buf;
    size_t          capacity, item_size;
    size_t          head, tail, count;
    int             closed;
    pthread_mutex_t mu;
    /* TODO: the condition variable(s) you need */
};

queue_t *queue_create(size_t capacity, size_t item_size) {
    /* TODO: allocate the queue and its buffer, initialize the mutex and condition variables */
    (void)capacity;
    (void)item_size;
    return NULL;
}

int queue_push(queue_t *q, const void *item) {
    /* TODO: lock; while full and not closed: wait; if closed: unlock, return -1;
     * copy the item to buf + tail * item_size; advance tail; count++; signal; unlock */
    (void)q;
    (void)item;
    return -1;
}

int queue_try_push(queue_t *q, const void *item) {
    /* TODO: like queue_push, but return -1 immediately if the queue is full */
    (void)q;
    (void)item;
    return -1;
}

int queue_pop(queue_t *q, void *out) {
    /* TODO: lock; while empty and not closed: wait; if empty (so closed): unlock, return -1;
     * copy the item at head to out; advance head; count--; signal; unlock */
    (void)q;
    (void)out;
    return -1;
}

void queue_close(queue_t *q) {
    /* TODO: lock; closed = 1; wake EVERYONE (broadcast, not signal); unlock */
    (void)q;
}

size_t queue_size(queue_t *q) {
    /* TODO */
    (void)q;
    return 0;
}

void queue_destroy(queue_t *q) {
    /* TODO: destroy the mutex and condition variables, free the buffer and the queue.
     * The caller guarantees that no thread is still using the queue. */
    (void)q;
}
