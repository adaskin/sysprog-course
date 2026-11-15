/* queue.h — bounded thread-safe queue (given interface; YOU implement queue.c, Task 1). */
#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct queue queue_t;

queue_t *queue_create(size_t capacity, size_t item_size);   /* copies items by value */
int      queue_push(queue_t *q, const void *item);          /* blocks while full; 0 ok, -1 if closed */
int      queue_try_push(queue_t *q, const void *item);      /* 0 ok, -1 if full or closed */
int      queue_pop(queue_t *q, void *out);                  /* blocks while empty; 0 ok;
                                                               -1 once closed AND drained */
void     queue_close(queue_t *q);                           /* wake everyone; pushes now fail */
size_t   queue_size(queue_t *q);                            /* current number of items */
void     queue_destroy(queue_t *q);

#endif
