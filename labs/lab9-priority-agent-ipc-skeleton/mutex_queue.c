#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "agentlab.h"

int mutex_submit(void* state, const task_t* task);
int mutex_receive(void* state, task_t* task);
void mutex_destroy(void* state);

typedef struct {
    int capacity;
    int count;
    task_t tasks[QUEUE_CAPACITY];
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} mutex_queue_t;

/* Given: initialization and destruction. */
backend_t mutex_backend(int capacity) {
    if (capacity <= 0 || capacity > QUEUE_CAPACITY) {
        errno = EINVAL;
        perror("mutex queue capacity");
        return (backend_t){0};
    }

    mutex_queue_t* queue = calloc(1, sizeof(*queue));
    if (queue == NULL) {
        perror("calloc mutex queue");
        return (backend_t){0};
    }
    queue->capacity = capacity;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return (backend_t){
        .state = queue,
        .submit = mutex_submit,
        .receive = mutex_receive,
        .destroy = mutex_destroy,
        .process_agents = 0,
    };
}

/* TODO 3: Submit while holding the mutex; wait while the queue is
 * full. */
int mutex_submit(void* state, const task_t* task) {
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

/*
 * TODO 4: Receive while holding the mutex; wait while the queue is
 * empty. Use best_task_index(), remove that array entry, and wake one
 * submitter.
 */
int mutex_receive(void* state, task_t* task) {
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

void mutex_destroy(void* state) {
    mutex_queue_t* queue = state;
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->lock);
    free(queue);
}
