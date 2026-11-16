#include "agentlab.h"

#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>

int shm_submit(void *state, const task_t *task);
int shm_receive(void *state, task_t *task);
void shm_destroy(void *state);

typedef struct {
    int capacity;
    int count;
    sem_t lock;             /* process-shared binary semaphore */
    sem_t empty_slots;      /* initially capacity */
    sem_t items;            /* initially 0 */
    task_t tasks[QUEUE_CAPACITY];
} shm_queue_t;

/* Given: shared mapping and process-shared semaphore initialization. */
backend_t shm_backend(int capacity)
{
    if (capacity <= 0 || capacity > QUEUE_CAPACITY) {
        errno = EINVAL;
        perror("shared queue capacity");
        return (backend_t){0};
    }

    shm_queue_t *queue = mmap(NULL, sizeof(*queue), PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (queue == MAP_FAILED) {
        perror("mmap shared queue");
        return (backend_t){0};
    }
    queue->capacity = capacity;
    queue->count = 0;

    /* pshared == 1: agents in child processes must share these semaphores. */
    if (sem_init(&queue->lock, 1, 1) < 0 ||
        sem_init(&queue->empty_slots, 1, (unsigned)capacity) < 0 ||
        sem_init(&queue->items, 1, 0) < 0) {
        perror("sem_init");
        munmap(queue, sizeof(*queue));
        return (backend_t){0};
    }

    return (backend_t){
        .state = queue,
        .submit = shm_submit,
        .receive = shm_receive,
        .destroy = shm_destroy,
        .process_agents = 1,
    };
}

/*
 * TODO 7: Wait for an empty slot, lock the array, append the task, unlock,
 * then post items. Retry sem_wait() when it fails with EINTR.
 */
int shm_submit(void *state, const task_t *task)
{
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

/*
 * TODO 8: Wait for an item, lock the array, remove the best task, unlock,
 * then post empty_slots. Use the same best_task_index() as the mutex queue.
 */
int shm_receive(void *state, task_t *task)
{
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

void shm_destroy(void *state)
{
    shm_queue_t *queue = state;
    sem_destroy(&queue->items);
    sem_destroy(&queue->empty_slots);
    sem_destroy(&queue->lock);
    munmap(queue, sizeof(*queue));
}
