#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "agentlab.h"

/* Given: command-line handling, timing, cleanup, and correctness
 * checks. */

static int positive_number(const char* text, const char* name) {
    char* end;
    long value = strtol(text, &end, 10);

    if (*text == '\0' || *end != '\0' || value <= 0 ||
        value > 1000000) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(EXIT_FAILURE);
    }
    return (int)value;
}

static backend_t open_backend(const char* name) {
    if (strcmp(name, "mutex") == 0) {
        return mutex_backend(QUEUE_CAPACITY);
    }
    if (strcmp(name, "mq") == 0) {
        return mq_backend(QUEUE_CAPACITY);
    }
    if (strcmp(name, "shm") == 0) {
        return shm_backend(QUEUE_CAPACITY);
    }
    return (backend_t){0};
}

/* Submit a batch before receiving it: the queued order must follow
 * the rules. */
static int priority_order_ok(const char* name) {
    backend_t backend = open_backend(name);
    int ok = 1;

    if (backend.state == NULL) {
        return 0;
    }
    for (int i = 0; i < 12; ++i) {
        task_t task = make_task(i);
        if (backend.submit(backend.state, &task) < 0) {
            perror("priority check submit");
            ok = 0;
            break;
        }
    }

    unsigned previous_priority = UINT_MAX;
    uint64_t previous_sequence = 0;
    for (int i = 0; ok && i < 12; ++i) {
        task_t task;
        if (backend.receive(backend.state, &task) < 0) {
            perror("priority check receive");
            ok = 0;
            break;
        }
        if (task.priority > previous_priority ||
            (task.priority == previous_priority &&
             task.sequence <= previous_sequence)) {
            fprintf(stderr,
                    "%s: priority or tie order is incorrect\n", name);
            ok = 0;
            break;
        }
        previous_priority = task.priority;
        previous_sequence = task.sequence;
    }

    backend.destroy(backend.state);
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s mutex|mq|shm [agents [tasks]]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int agent_count =
        argc >= 3 ? positive_number(argv[2], "agent count") : 4;
    int task_count =
        argc >= 4 ? positive_number(argv[3], "task count") : 20000;
    backend_t backend;

    if (agent_count > QUEUE_CAPACITY) {
        fprintf(stderr, "agent count must be at most %d\n",
                QUEUE_CAPACITY);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "mutex") != 0 && strcmp(argv[1], "mq") != 0 &&
        strcmp(argv[1], "shm") != 0) {
        fprintf(stderr, "unknown backend: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (!priority_order_ok(argv[1])) {
        fprintf(stderr, "%s: priority queue check failed\n", argv[1]);
        return EXIT_FAILURE;
    }

    backend = open_backend(argv[1]);
    if (backend.state == NULL) {
        return EXIT_FAILURE;
    }

    size_t results_size = (size_t)task_count * sizeof(result_t);
    result_t* results =
        mmap(NULL, results_size, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (results == MAP_FAILED) {
        perror("mmap results");
        backend.destroy(backend.state);
        return EXIT_FAILURE;
    }

    double start = monotonic_ms();
    int status = run_pool(&backend, agent_count, task_count, results);
    double elapsed = monotonic_ms() - start;

    unsigned completed = 0;
    unsigned incorrect = 0;
    uint64_t checksum = 0;
    for (int i = 0; i < task_count; ++i) {
        if (results[i].done == 1) {
            ++completed;
            checksum ^= results[i].value;
        } else {
            ++incorrect;
        }
    }

    backend.destroy(backend.state);
    if (munmap(results, results_size) < 0) {
        perror("munmap results");
        return EXIT_FAILURE;
    }

    if (status != 0 || incorrect != 0) {
        fprintf(stderr, "%s: completed=%u incorrect_or_missing=%u\n",
                argv[1], completed, incorrect);
        return EXIT_FAILURE;
    }

    printf("%s,%d,%d,%.3f,%.1f,%016llx\n", argv[1], agent_count,
           task_count, elapsed, task_count * 1000.0 / elapsed,
           (unsigned long long)checksum);
    return EXIT_SUCCESS;
}
