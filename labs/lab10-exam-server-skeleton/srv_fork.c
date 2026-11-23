/* srv_fork.c -- YOUR FILE. One process per connection (TODO 5). */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "conn.h"
#include "srv.h"

/* TODO 5: process-per-connection server.
 *
 * Required behavior:
 *   - accept() in a loop until g_stop (retry on EINTR).
 *   - fork() per client. The CHILD closes the listening socket, serves its
 *     connection (conn_read -> conn_serve loop), closes it and _exit(0)s.
 *     The PARENT closes its copy of the client fd immediately.
 *   - No zombies: use signal(SIGCHLD, SIG_IGN) or a SIGCHLD handler with
 *     waitpid(-1, NULL, WNOHANG) in a loop. Check with: ps aux | grep defunct
 *   - No file-descriptor leak in the parent (it must close every client fd).
 *   - Shutdown: when g_stop is set, return 0. (Bonus: kill and wait for children.)
 *
 * Questions for NOTES.md: why do duplicate submissions still get rejected
 * although the processes share no memory? Which cost does fork() add compared
 * to a thread? */
int run_fork(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_fork: TODO 5 not implemented\n");
    return -1;
}
