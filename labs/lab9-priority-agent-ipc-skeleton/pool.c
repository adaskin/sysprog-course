#include <stdio.h>

#include "agentlab.h"

/*
 * TODO 1: Complete the common agent loop.
 *
 * Repeatedly receive one task from the backend. A task with id == -1
 * is a stop task. For a normal task, run it and store value, agent,
 * and done = 1 in results[task.id]. Each task id has its own result
 * slot, so no results lock is needed. Return NULL on a receive error
 * or after a stop task.
 */
void* agent_loop(void* arg) {
    (void)arg;
    fprintf(stderr, "TODO: implement agent_loop\n");
    return NULL;
}

/*
 * TODO 2: Complete the submitter thread.
 *
 * Create tasks with make_task(0), ..., make_task(task_count - 1),
 * submitting each one through the backend. The runner sends the stop
 * tasks later. Return NULL immediately if a submit operation fails.
 */
void* submitter_loop(void* arg) {
    (void)arg;
    fprintf(stderr, "TODO: implement submitter_loop\n");
    return NULL;
}
