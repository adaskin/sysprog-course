/* chan_fifo.c — the control channel as a named pipe (Task 3: YOUR CODE).
 *
 * Address "fifo:NAME" means the FIFO /tmp/NAME.ctl. Messages are text lines, like the fd
 * transport in chan.c (look at it: you can reuse LineReader and send_all from line_io.h).
 *
 *   CHAN_RECV (the center, one receiver):
 *     - create the FIFO with mkfifo (if a stale one is left by a crashed center, remove it first)
 *     - open it for reading. Two traps, both from Lab 5:
 *         * open(O_RDONLY) blocks until some writer opens the FIFO;
 *         * when the last writer closes, read() returns 0 (EOF), and every later read() too.
 *       A center must survive operators that come and go. One classic answer: open the read
 *       end with O_NONBLOCK, then open a write end yourself and keep it (a "dummy writer"),
 *       then clear O_NONBLOCK on the read end (fcntl). Other answers are fine; explain yours.
 *     - recv: one line, waiting at most timeout_ms (poll, as in chan.c)
 *     - close: close both descriptors and unlink the FIFO
 *   CHAN_SEND (swarmctl, a viewer button; many senders):
 *     - open(O_WRONLY | O_NONBLOCK): fails with ENXIO if no center is reading. Return NULL then,
 *       without a message: "no show is running" is for the caller to report.
 *     - send: the line and its '\n' in ONE write. Why does that matter with many senders?
 *       (man 7 pipe: PIPE_BUF)
 *
 * The center forks leaders after it opens this channel: use O_CLOEXEC, or a leader keeps your
 * FIFO open and you will wonder why things do not behave.
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>
#include <stdlib.h>

struct fifo_chan {
    struct chan base;           /* must be first */
    /* TODO: descriptors, the path, a LineReader, ... */
};

static int fifo_send(chan_t *c, const void *msg, size_t len) {
    (void)c;
    (void)msg;
    (void)len;
    return CHAN_CLOSED;         /* TODO (Task 3) */
}

static ssize_t fifo_recv(chan_t *c, void *buf, size_t cap, int timeout_ms) {
    (void)c;
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    return CHAN_CLOSED;         /* TODO (Task 3) */
}

static void fifo_close(chan_t *c) {
    free(c);                    /* TODO (Task 3) */
}

static const struct chan_ops fifo_ops = { .send = fifo_send, .recv = fifo_recv, .close = fifo_close };

chan_t *chan_fifo_open(const char *name, int mode) {
    (void)name;
    (void)fifo_ops;
    if (mode == CHAN_RECV) {
        fprintf(stderr, "chan_fifo: not implemented yet (Task 3)\n");
    }
    return NULL;                /* TODO (Task 3) */
}
