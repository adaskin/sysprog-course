#ifndef AGENTLAB_H
#define AGENTLAB_H

#include <stdint.h>

#define QUEUE_CAPACITY 32

/* Larger priority values run first. Priority 0 is reserved for stop
 * tasks. */
typedef struct {
    int id;            /* -1 means: agent, stop */
    unsigned priority; /* normal tasks: 1, 2, or 3 */
    uint64_t sequence; /* submission order; breaks equal-priority ties */
    unsigned work; /* deterministic amount of dummy work */
} task_t;

typedef struct {
    uint64_t value;
    unsigned done; /* must be exactly 1 after a correct run */
    int agent;
} result_t;

/*
 * A queue backend. implement one instance of this for each of
 * the three designs (mutex, mq, shm) and the common loops only ever
 * call through these pointers.
 *
 * submit(state, task):  enqueue one task. BLOCKS while the queue is
 * full. Returns 0 on success, -1 on error (sets errno).
 * receive(state, task): dequeue the highest-priority task (larger
 * priority first; ties broken by smaller sequence). BLOCKS while the
 * queue is empty. Returns 0 or -1. destroy(state):       release
 * queue resources. Called once, after all agents have exited. Must
 * not be called concurrently with submit/receive.
 *
 * Thread/process safety: submit and receive may be called
 * concurrently by one submitter and N agents. Implementations must
 * serialize access to their internal state.
 */
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
/* Implemented in pool.c. Both are submitted as pthread entries; for
 * process backends, agent_loop runs in a forked child. Return NULL on
 * completion. */
void* agent_loop(void* arg);
void* submitter_loop(void* arg);


/* Given runner and task helpers. */

/* Start agents and the submitter, drain, send stop tasks, join.
 * Returns 0 on success, non-zero on error. */
int run_pool(backend_t* backend, int agent_count, int task_count,
             result_t* results);
task_t make_task(int id);

/* A priority-0, id-(-1) sentinel. Agents stop when they receive one.
 */
task_t make_stop_task(uint64_t sequence);
uint64_t run_task(const task_t* task);

/*
 * Return the index of the task to run next: largest priority first,
 * then smallest sequence. Only for the array-backed queues (mutex and
 * shm); the mq backend lets the kernel schedule and must NOT call
 * this.
 */
int best_task_index(const task_t* tasks, int count);
double monotonic_ms(void);

#endif
