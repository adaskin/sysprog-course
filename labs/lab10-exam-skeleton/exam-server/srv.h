/* srv.h -- the five server models. Each run_* function is YOUR job.
 * Contract (same for all five):
 *   - listen_fd is already bound and listening (see main.c).
 *   - Serve clients with conn_read / conn_serve from server.h.
 *   - Return 0 when g_stop becomes non-zero (SIGINT/SIGTERM), closing every
 *     client socket you still have. Return -1 on a fatal setup error.
 *   - A bad or vanishing client must never stop the server. */
#ifndef SRV_H
#define SRV_H

#include <signal.h>

extern volatile sig_atomic_t g_stop;     /* set by the signal handler in main.c */

int run_iterative(int listen_fd); /* srv_iterative.c: one client at a time (TODO 4) */
int run_thread(int listen_fd);   /* srv_thread.c  : one thread per connection   */
int run_fork(int listen_fd);     /* srv_fork.c    : one process per connection  */
int run_select(int listen_fd);   /* srv_select.c  : one thread, select()        */
int run_poll(int listen_fd);     /* srv_poll.c    : one thread, poll()          */
int run_epoll(int listen_fd);    /* srv_epoll.c   : one thread, epoll (Linux)   */

#endif
