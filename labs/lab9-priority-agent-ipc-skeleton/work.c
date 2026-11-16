#include "agentlab.h"

#include <time.h>

/* Given: identical tasks and work for all three backends. */
task_t make_task(int id)
{
    task_t task = {
        .id = id,
        .priority = (unsigned)((id * 13) % 3) + 1,
        .sequence = (uint64_t)id,
        .work = 500u + (unsigned)((id * 97) % 1500),
    };
    return task;
}

task_t make_stop_task(uint64_t sequence)
{
    task_t task = {
        .id = -1,
        .priority = 0,
        .sequence = sequence,
        .work = 0,
    };
    return task;
}

uint64_t run_task(const task_t *task)
{
    uint64_t x = (uint64_t)(unsigned)task->id + 0x9e3779b97f4a7c15ULL;

    for (unsigned i = 0; i < task->work; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    return x;
}

/* Given: used by the mutex and shared-memory array queues. */
int best_task_index(const task_t *tasks, int count)
{
    int best = 0;

    for (int i = 1; i < count; ++i) {
        if (tasks[i].priority > tasks[best].priority ||
            (tasks[i].priority == tasks[best].priority &&
             tasks[i].sequence < tasks[best].sequence)) {
            best = i;
        }
    }
    return best;
}

double monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
