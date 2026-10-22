/* common.h — Lab 7 helpers (given). */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_AGENTS 64

/* Wall-clock seconds from a monotonic clock. */
static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Abort with a message if a pthread call fails (they return the error code). */
#define CHECK(call)                                                        \
    do {                                                                   \
        int rc_ = (call);                                                  \
        if (rc_ != 0) {                                                    \
            fprintf(stderr, "%s:%d: %s failed: %d\n", __FILE__, __LINE__,  \
                    #call, rc_);                                           \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

#endif
