/* test_queue.c — checks of YOUR queue.c (given). */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "queue.h"

static int failures;
#define CHECK_TRUE(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void test_order_and_full(void) {
    queue_t *q = queue_create(3, sizeof(int));
    CHECK_TRUE(q != NULL);
    int v;
    for (v = 1; v <= 3; v++) {
        CHECK_TRUE(queue_push(q, &v) == 0);
    }
    v = 4;
    CHECK_TRUE(queue_try_push(q, &v) == -1);          /* full */
    CHECK_TRUE(queue_size(q) == 3);
    for (int want = 1; want <= 3; want++) {
        CHECK_TRUE(queue_pop(q, &v) == 0 && v == want);
    }
    CHECK_TRUE(queue_try_push(q, &v) == 0);           /* space again */
    queue_destroy(q);
}

static queue_t *gq;
static void *delayed_push(void *arg) {
    (void)arg;
    usleep(100000);
    int v = 42;
    queue_push(gq, &v);
    return NULL;
}

static void test_pop_blocks(void) {
    gq = queue_create(2, sizeof(int));
    pthread_t t;
    pthread_create(&t, NULL, delayed_push, NULL);
    int v = 0;
    CHECK_TRUE(queue_pop(gq, &v) == 0 && v == 42);     /* must have waited, not spun or failed */
    pthread_join(t, NULL);
    queue_destroy(gq);
}

static void *delayed_close(void *arg) {
    (void)arg;
    usleep(100000);
    queue_close(gq);
    return NULL;
}

static void test_close_wakes_and_drains(void) {
    gq = queue_create(4, sizeof(int));
    int v = 7;
    queue_push(gq, &v);
    queue_push(gq, &v);
    queue_close(gq);
    CHECK_TRUE(queue_push(gq, &v) == -1);
    CHECK_TRUE(queue_try_push(gq, &v) == -1);
    CHECK_TRUE(queue_pop(gq, &v) == 0);                /* drains what is left ... */
    CHECK_TRUE(queue_pop(gq, &v) == 0);
    CHECK_TRUE(queue_pop(gq, &v) == -1);               /* ... then fails */
    queue_destroy(gq);

    gq = queue_create(4, sizeof(int));
    pthread_t t;
    pthread_create(&t, NULL, delayed_close, NULL);
    CHECK_TRUE(queue_pop(gq, &v) == -1);               /* a blocked pop is woken by close */
    pthread_join(t, NULL);
    queue_destroy(gq);
}

static void *blocked_pusher(void *arg) {
    int v = 1;
    *(int *)arg = queue_push(gq, &v);                  /* queue is full: blocks, then close -> -1 */
    return NULL;
}

static void test_close_wakes_blocked_push(void) {
    gq = queue_create(1, sizeof(int));
    int v = 1, rc = 99;
    queue_push(gq, &v);
    pthread_t t;
    pthread_create(&t, NULL, blocked_pusher, &rc);
    usleep(100000);
    queue_close(gq);
    pthread_join(t, NULL);
    CHECK_TRUE(rc == -1);
    queue_destroy(gq);
}

#define PRODUCERS 4
#define CONSUMERS 3
#define PER_PRODUCER 20000

static long consumed_sum[CONSUMERS];
static long consumed_cnt[CONSUMERS];

static void *producer(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < PER_PRODUCER; i++) {
        int v = id * PER_PRODUCER + i;
        queue_push(gq, &v);
    }
    return NULL;
}

static void *consumer(void *arg) {
    int id = *(int *)arg;
    int v;
    while (queue_pop(gq, &v) == 0) {
        consumed_sum[id] += v;
        consumed_cnt[id]++;
    }
    return NULL;
}

static void test_many_threads(void) {
    gq = queue_create(8, sizeof(int));
    pthread_t pt[PRODUCERS], ct[CONSUMERS];
    int pid[PRODUCERS], cid[CONSUMERS];
    for (int i = 0; i < CONSUMERS; i++) {
        cid[i] = i;
        pthread_create(&ct[i], NULL, consumer, &cid[i]);
    }
    for (int i = 0; i < PRODUCERS; i++) {
        pid[i] = i;
        pthread_create(&pt[i], NULL, producer, &pid[i]);
    }
    for (int i = 0; i < PRODUCERS; i++) {
        pthread_join(pt[i], NULL);
    }
    queue_close(gq);
    for (int i = 0; i < CONSUMERS; i++) {
        pthread_join(ct[i], NULL);
    }
    long sum = 0, cnt = 0;
    for (int i = 0; i < CONSUMERS; i++) {
        sum += consumed_sum[i];
        cnt += consumed_cnt[i];
    }
    long total = (long)PRODUCERS * PER_PRODUCER;
    CHECK_TRUE(cnt == total);
    CHECK_TRUE(sum == total * (total - 1) / 2);        /* every value exactly once */
    queue_destroy(gq);
}

int main(void) {
    test_order_and_full();
    test_pop_blocks();
    test_close_wakes_and_drains();
    test_close_wakes_blocked_push();
    test_many_threads();
    if (failures) {
        printf("test_queue: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_queue: all passed\n");
    return 0;
}
