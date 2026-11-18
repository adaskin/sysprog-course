#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "agentlab.h"

/* Given: thread/process creation, submitter startup, stop tasks, and
 * joins. */

static void pthread_ok(int error, const char* operation) {
    if (error != 0) {
        fprintf(stderr, "%s: pthread error %d\n", operation, error);
        exit(EXIT_FAILURE);
    }
}

int run_pool(backend_t* backend, int agent_count, int task_count,
             result_t* results) {
    pthread_t* threads =
        calloc((size_t)agent_count, sizeof(*threads));
    pid_t* children = calloc((size_t)agent_count, sizeof(*children));
    agent_arg_t* agent_args =
        calloc((size_t)agent_count, sizeof(*agent_args));
    submitter_arg_t submitter_arg = {
        .backend = backend,
        .task_count = task_count,
    };
    pthread_t submitter;

    if (threads == NULL || children == NULL || agent_args == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < agent_count; ++i) {
        agent_args[i].backend = backend;
        agent_args[i].results = results;
        agent_args[i].agent_id = i;

        if (backend->process_agents) {
            children[i] = fork();
            if (children[i] < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (children[i] == 0) {
                agent_loop(&agent_args[i]);
                _exit(0);
            }
        } else {
            pthread_ok(pthread_create(&threads[i], NULL, agent_loop,
                                      &agent_args[i]),
                       "pthread_create agent");
        }
    }

    pthread_ok(pthread_create(&submitter, NULL, submitter_loop,
                              &submitter_arg),
               "pthread_create submitter");
    pthread_ok(pthread_join(submitter, NULL),
               "pthread_join submitter");

    /* Priority 0 makes every normal task leave the queue before a
     * stop task. */
    for (int i = 0; i < agent_count; ++i) {
        task_t stop =
            make_stop_task((uint64_t)task_count + (uint64_t)i);
        if (backend->submit(backend->state, &stop) < 0) {
            perror("submit stop task");
            return 1;
        }
    }

    for (int i = 0; i < agent_count; ++i) {
        if (backend->process_agents) {
            if (waitpid(children[i], NULL, 0) < 0) {
                perror("waitpid");
                return 1;
            }
        } else {
            pthread_ok(pthread_join(threads[i], NULL),
                       "pthread_join agent");
        }
    }

    free(agent_args);
    free(children);
    free(threads);
    return 0;
}
