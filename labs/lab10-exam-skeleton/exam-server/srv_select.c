/* srv_select.c -- YOUR FILE. One thread, select() event loop (TODO 9). */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "server.h"
#include "srv.h"

#include <sys/select.h>

/* TODO 9: event loop with select().   (homework)
 *
 * Required behavior:
 *   - ONE thread. Keep a table of conn_t* indexed by fd; all client sockets
 *     are O_NONBLOCK.
 *   - Every iteration: build a fresh fd_set (FD_ZERO + FD_SET for the listener
 *     and each client), call select() with a ~1 s timeout (so g_stop is
 *     checked), handle EINTR.
 *   - Listener readable -> accept(); new client -> allocate a conn_t, conn_init,
 *     make it non-blocking.
 *   - Client readable -> conn_read(): >0 -> conn_serve(); 0, a hard error, or
 *     conn_serve()==-1 -> close and free; EAGAIN/EINTR -> ignore.
 *   - FD_SETSIZE is 1024: a client whose fd >= FD_SETSIZE must be refused
 *     (close it immediately); the server must NOT crash or corrupt memory.
 *   - Shutdown: when g_stop is set, close all clients, free memory, return 0.
 *
 * Questions for NOTES.md: why must the fd_set be rebuilt every time? What is
 * the cost of one select() call with 500 idle clients? */
int run_select(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_select: TODO 9 not implemented\n");
    return -1;
}
