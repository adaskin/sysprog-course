/* srv_epoll.c -- YOUR FILE. One thread, Linux epoll event loop (TODO 7). */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "server.h"
#include "srv.h"

#include <sys/epoll.h>

/* TODO 7: event loop with epoll (level-triggered is enough).
 *
 * Required behavior:
 *   - epoll_create1(0); register the listener (EPOLLIN) and every accepted,
 *     NON-BLOCKING client socket (EPOLLIN | EPOLLRDHUP). Store the conn_t* in
 *     epoll_event.data.ptr (the listener needs a way to be told apart,
 *     e.g. data.ptr == NULL).
 *   - epoll_wait() with a ~1 s timeout; handle EINTR; stop when g_stop.
 *   - Same per-client logic as select()/poll(): conn_read, conn_serve, close
 *     on EOF/error. When you close a client, epoll forgets it automatically,
 *     but free its conn_t only once.
 *   - 800 simultaneous clients must work.
 *
 * Bonus A: edge-triggered (EPOLLET) -- then you MUST read until EAGAIN.
 * Bonus B: replace the blocking-style send_all with a per-connection output
 *          buffer and EPOLLOUT, so a slow reader cannot stall the loop.
 *
 * Questions for NOTES.md: which O(n) cost of poll() disappeared? Why can a
 * single slow handler (try EXAM_GRADE_DELAY_MS=50) hurt ALL clients in the
 * three event-loop servers but only one client in thread/fork? */
int run_epoll(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_epoll: TODO 7 not implemented\n");
    return -1;
}
