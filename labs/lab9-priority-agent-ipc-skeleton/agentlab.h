#ifndef AGENTLAB_H
#define AGENTLAB_H

#include <stdint.h>

#define QUEUE_CAPACITY 32

/* Larger priority values run first. Priority 0 is reserved for stop
 * tasks. */
typedef struct {
    int id;            /* -1 means: agent, stop */
    unsigned priority; /* normal tasks: 1, 2, or 3 */
    uint64_t
        sequence;  /* submission order; breaks equal-priority ties */
    unsigned work; /* deterministic amount of dummy work */
} task_t;

typedef struct {
    uint64_t value;
    unsigned done; /* must be exactly 1 after a correct run */
    int agent;
} result_t;

typedef struct backend {
    void* state;
    int (*submit)(void* state, const task_t* task);
    int (*receive)(void* state, task_t* task);
    void (*destroy)(void* state);
    int process_agents; /* 0: thread agents, 1: process agents */
} backend_t;

typedef struct {
    backend_t* backend;
    result_t* results;
    int agent_id;
} agent_arg_t;

typedef struct {
    backend_t* backend;
    int task_count;
} submitter_arg_t;

/* The three backends students complete. */
backend_t mutex_backend(int capacity);
backend_t mq_backend(int capacity);
backend_t shm_backend(int capacity);

/* Student code: the common agent pool behavior. */
void* agent_loop(void* arg);
void* submitter_loop(void* arg);

/* Given runner and task helpers. */
int run_pool(backend_t* backend, int agent_count, int task_count,
             result_t* results);
task_t make_task(int id);
task_t make_stop_task(uint64_t sequence);
uint64_t run_task(const task_t* task);
int best_task_index(const task_t* tasks, int count);
double monotonic_ms(void);

#endif
