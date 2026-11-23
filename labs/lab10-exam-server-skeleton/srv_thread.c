/* srv_thread.c -- YOUR FILE. One thread per connection (TODO 4). */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "conn.h"
#include "srv.h"

/* TODO 4: thread-per-connection server.
 *
 * Required behavior:
 *   - accept() in a loop until g_stop; retry on EINTR; keep going on other
 *     accept errors (e.g. ECONNABORTED).
 *   - For each client create a DETACHED thread (or join them all at shutdown).
 *   - The thread owns its own conn_t and client fd; it loops conn_read ->
 *     conn_serve until the peer closes, an error occurs, or conn_serve says -1,
 *     then close()s the fd. Nothing may leak (check with: ls /proc/PID/fd | wc -l).
 *   - Do NOT pass the address of a loop variable to the thread (data race!).
 *   - Shutdown: when g_stop is set, stop accepting and return 0.
 *     (Bonus: keep a counter of live threads and wait until it is 0.)
 *
 * Questions for NOTES.md: what is shared between threads here and what is not?
 * Why is no mutex needed anywhere in this lab? */
int run_thread(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_thread: TODO 4 not implemented\n");
    return -1;
}
