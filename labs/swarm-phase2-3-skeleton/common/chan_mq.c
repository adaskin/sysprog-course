/* chan_mq.c — the control channel as a POSIX message queue (Task 3, OPTIONAL alternative).
 *
 * If you prefer a message queue to a FIFO, implement this file and set CTL_TRANSPORT to "mq" in
 * chan.h. Address "mq:NAME" means the queue "/NAME.ctl" (mq_open; see Lecture 9, man 7 mq_overview).
 *
 *   CHAN_RECV (the center): mq_unlink a stale queue, then mq_open(O_CREAT | O_RDONLY | O_CLOEXEC)
 *     with mq_maxmsg = 10 and mq_msgsize = MAX_LINE; recv with mq_timedreceive (absolute
 *     CLOCK_REALTIME deadline: chan_deadline in chan_impl.h); close: mq_close + mq_unlink.
 *   CHAN_SEND: mq_open(O_WRONLY | O_NONBLOCK); NULL if the queue does not exist (no center).
 *     A full queue: decide whether send fails or waits a little, and say why in REPORT.md.
 *   A message is one command, without '\n'. Compare with the FIFO in your report: what does each
 *   one give you for free, and what would change in Phase 3 when commands come over TCP?
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>

chan_t *chan_mq_open(const char *name, int mode) {
    (void)name;
    (void)mode;
    fprintf(stderr, "chan_mq: not implemented (optional, Task 3)\n");
    return NULL;
}
