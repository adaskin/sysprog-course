#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "agentlab.h"

int mq_queue_submit(void* state, const task_t* task);
int mq_queue_receive(void* state, task_t* task);
void mq_destroy(void* state);

typedef struct {
    mqd_t descriptor;
    char name[64];
} mq_queue_t;

/* Given: create a named kernel queue inherited by the process agents.
 */
backend_t mq_backend(int capacity) {
    mq_queue_t* queue = calloc(1, sizeof(*queue));
    if (queue == NULL) {
        perror("calloc message queue");
        return (backend_t){0};
    }

    snprintf(queue->name, sizeof(queue->name),
             "/bil322_agents_%ld_%ld", (long)getuid(),
             (long)getpid());
    mq_unlink(queue->name); /* Remove only a stale queue with our
                               unique name. */

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = capacity,
        .mq_msgsize = sizeof(task_t),
        .mq_curmsgs = 0,
    };
    queue->descriptor =
        mq_open(queue->name, O_CREAT | O_RDWR, 0600, &attr);
    if (queue->descriptor == (mqd_t)-1) {
        perror("mq_open");
        free(queue);
        return (backend_t){0};
    }

    return (backend_t){
        .state = queue,
        .submit = mq_queue_submit,
        .receive = mq_queue_receive,
        .destroy = mq_destroy,
        .process_agents = 1,
    };
}

/*
 * TODO 5: Send one whole task_t with mq_send().
 * Use task->priority as the kernel message priority. Retry if
 * interrupted.
 */
int mq_queue_submit(void* state, const task_t* task) {
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

/*
 * TODO 6: Receive one whole task_t with mq_receive().
 * The kernel performs the priority scheduling for this backend.
 */
int mq_queue_receive(void* state, task_t* task) {
    (void)state;
    (void)task;
    errno = ENOSYS;
    return -1;
}

void mq_destroy(void* state) {
    mq_queue_t* queue = state;
    mq_close(queue->descriptor);
    mq_unlink(queue->name);
    free(queue);
}
