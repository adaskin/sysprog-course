/* chan_shm.c — the stage bus: a "newest value wins" channel in shared memory (Task 1: YOUR CODE).
 *
 * One sender (the center) and any number of receivers (viewers, swarmctl), on one machine.
 * A message is at most SHM_MAX_MSG bytes (a Frame). The rules, which the tests check whatever
 * design you choose (justify yours in REPORT.md):
 *
 *   1. chan_send never blocks and never waits for a receiver. A receiver that is slow, stopped
 *      (SIGSTOP) or killed (SIGKILL) at any moment cannot slow down or freeze the sender.
 *   2. A receiver gets whole messages only: never half old, half new (a "torn" frame).
 *   3. A receiver gets the newest message it has not seen yet; it may skip older ones. A receiver
 *      that attaches gets the current message at once (a viewer started late sees the show).
 *   4. A receiver waits without busy-waiting, and never longer than its timeout_ms.
 *   5. After the sender's chan_close, every receiver gets the last message (if it had not seen it
 *      yet) and then CHAN_CLOSED. If the sender dies without closing, receivers notice it
 *      (CHAN_CLOSED) within about a second, and the next sender with the same name can start.
 *   6. Receivers map the message area READ-ONLY (PROT_READ): a buggy viewer cannot damage the
 *      show. (A small read-write area, for example one wake-up slot per receiver, is allowed.)
 *   7. After the sender's chan_close, nothing is left in /dev/shm.
 *
 * A suggested design (you may choose another one):
 *
 *   shm object "/NAME", made of two parts, each starting on a page boundary:
 *
 *   page 0, read-write for everybody: the CONTROL part
 *       magic, version          refuse to attach to something else
 *       sender_pid              receivers check kill(sender_pid, 0) on a timeout (rule 5)
 *       closed                  set by the sender's chan_close
 *       slot[SHM_MAX_RECEIVERS] one per receiver: { pid; sem_t wake; }
 *                               a receiver takes a free slot (atomic compare-and-swap on pid)
 *                               and sleeps in sem_timedwait(&slot.wake); the sender posts the
 *                               semaphore of every used slot after each message, but only if its
 *                               value is 0 (sem_getvalue), so a sleeping receiver never piles up
 *                               posts. sem_post never blocks (rule 1). Initialize each semaphore
 *                               with sem_init(&s, 1, 0): the 1 means "shared between processes".
 *
 *   page 1..: written by the sender, read-only for receivers: the MESSAGE part
 *       seq                     a sequence lock: odd while the sender is writing, even when the
 *                               message is complete; seq/2 numbers the messages
 *       len, data[SHM_MAX_MSG]  the newest message
 *
 *   sender:   seq++ (now odd); copy len and data; seq++ (now even); wake the receivers
 *   receiver: s1 = seq; if s1 is odd, try again; copy len and data out; s2 = seq;
 *             if s1 != s2 the sender wrote meanwhile: try again (rule 2)
 *             if s1/2 is not newer than the last message you returned: wait on your slot
 *
 *   Use C11 atomics (<stdatomic.h>) for seq, pid and closed: atomic_load/atomic_store/
 *   atomic_compare_exchange_strong work between processes on shared memory, and atomic_thread_fence
 *   orders the plain memcpy against them. Lecture 9 (IPC) has shm_open, ftruncate, mmap and
 *   semaphores in shared memory; `man 7 shm_overview`, `man sem_init`.
 *
 * Remember: shm_open names start with '/'; mmap offsets must be page-aligned
 * (sysconf(_SC_PAGESIZE)); close the descriptor after mmap (the mapping stays); the center forks
 * leaders after it opens the bus, so open with O_CLOEXEC.
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include "frame.h"

#define SHM_MAX_MSG        sizeof(Frame)
#define SHM_MAX_RECEIVERS  8

struct shm_chan {
    struct chan base;           /* must be first */
    /* TODO: what each end needs: the mappings, their sizes, the name, your slot, the number of
     * the last message you returned, ... */
};

static int shm_send(chan_t *c, const void *msg, size_t len) {
    (void)c;
    (void)msg;
    (void)len;
    return CHAN_CLOSED;         /* TODO (Task 1) */
}

static ssize_t shm_recv(chan_t *c, void *buf, size_t cap, int timeout_ms) {
    (void)c;
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    return CHAN_CLOSED;         /* TODO (Task 1) */
}

static void shm_close(chan_t *c) {
    free(c);                    /* TODO (Task 1): sender: mark closed, wake everyone, unmap,
                                   shm_unlink. Receiver: give the slot back, unmap. */
}

static const struct chan_ops shm_ops = { .send = shm_send, .recv = shm_recv, .close = shm_close };

chan_t *chan_shm_open(const char *name, int mode) {
    (void)name;
    (void)shm_ops;
    if (mode == CHAN_SEND) {
        fprintf(stderr, "chan_shm: not implemented yet (Task 1)\n");
    }
    return NULL;                /* TODO (Task 1) */
}
