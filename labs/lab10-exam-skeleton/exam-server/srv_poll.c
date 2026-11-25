/* srv_poll.c -- YOUR FILE. One thread, poll() event loop (TODO 6). */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "server.h"
#include "srv.h"

#include <poll.h>

/* TODO 6: event loop with poll().
 *
 * Required behavior: same as select(), but
 *   - keep a dynamic array of struct pollfd plus a parallel array of conn_t*
 *     (grow with realloc; remove a closed client by swapping the last entry
 *     into its slot -- careful while iterating!);
 *   - there is NO fd limit like FD_SETSIZE: 800 clients at once must work (loadtest --clients 800 --concurrency 800);
 *   - treat POLLHUP / POLLERR / POLLNVAL as "close this client";
 *   - poll() timeout ~1 s, handle EINTR, stop when g_stop.
 *
 * Questions for NOTES.md: what did you not have to rebuild each iteration
 * compared to select()? What still costs O(n) per poll() call? */
int run_poll(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_poll: TODO 6 not implemented\n");
    return -1;
}
