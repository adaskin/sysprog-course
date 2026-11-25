/* srv_iterative.c -- YOUR FILE. The simplest server: ONE client at a time (TODO 4). */
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"
#include "srv.h"

/* TODO 4: the first complete exam server.
 *
 *   loop until g_stop:
 *     accept() a client (retry on EINTR);
 *     conn_init() a conn_t on the stack;
 *     serve it until it is finished:  conn_read() -> conn_serve(), stop on
 *       EOF (read returns 0), an error (not EINTR), or conn_serve() == -1;
 *     close() the client fd, then accept the next one.
 *
 * Try it with ONE student (bin/student ...), then with several students started
 * together (tools/run_students.sh 5): what do the other students experience while
 * the first one is "thinking"? This is the problem the other models solve.
 *
 * Questions for NOTES.md: how long does 20 students take, and why? */
int run_iterative(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_iterative: TODO 4 not implemented\n");
    return -1;
}
