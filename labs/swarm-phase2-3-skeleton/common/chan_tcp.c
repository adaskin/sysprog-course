/* chan_tcp.c — the tcp: transport (Phase 3, Task 1: YOUR CODE).
 *
 * Three functions, called by chan.c:
 *
 *   chan_tcp_open("HOST:PORT", CHAN_BOTH)   connect; returns a line channel     (chan_open "tcp:...")
 *   chan_tcp_listen("HOST:PORT" or ":PORT") socket, bind, listen; a listener    (chan_listen)
 *   the listener's accept operation         one new connection, a line channel  (chan_accept)
 *
 * A connected socket is just a descriptor: once you have one, return chan_fd(sock, sock). The fd
 * transport in chan.c already does line framing, partial reads (LineReader), short writes
 * (send_all), too-long lines (CHAN_TOOBIG), chan_shutdown and chan_addr. So most of this file is
 * about getting a connected socket, quickly and safely.
 *
 * Rules (tests/test_chan tcp checks them):
 *
 *   1. Addresses: use chan_split_hostport (chan_impl.h; it handles "[::1]:9000") and getaddrinfo,
 *      so a host name, an IPv4 or an IPv6 address all work. Try each result until one works.
 *      ":PORT" (no host) for the listener means every interface; port 0 means "any free port"
 *      (the tests use it; chan_addr(listener, CHAN_LOCAL, ...) then tells which port you got).
 *   2. Every socket is close-on-exec (SOCK_CLOEXEC in socket() and accept4()): the center forks.
 *   3. The listener sets SO_REUSEADDR before bind, so a restarted center can bind at once.
 *   4. Connecting has a time limit (about 3 s): a non-blocking connect(), then poll() for POLLOUT,
 *      then getsockopt(SO_ERROR) tells whether it worked. Restore blocking mode afterwards.
 *      A port where nobody listens fails at once (ECONNREFUSED): return NULL, do not retry here,
 *      and leave errno as connect() set it (save it around close()): callers look at it.
 *   5. A peer that never reads must not block a sender forever: set SO_SNDTIMEO (about 2 s) on
 *      every connected socket, so send fails and chan_send returns CHAN_CLOSED. Also consider
 *      TCP_NODELAY: our messages are small lines and Nagle's algorithm would delay them.
 *   6. SIGPIPE: writing to a socket whose peer has closed raises SIGPIPE, which kills the process
 *      by default. Either ignore SIGPIPE in every main (the given mains do), or send with
 *      MSG_NOSIGNAL (that would mean your own send function instead of send_all). Say which, and
 *      why, in REPORT.md.
 *   7. The listener is a channel too, with its own struct and operations (see chan_impl.h): accept
 *      waits up to timeout_ms with poll() (NULL on timeout, -1 = forever), shutdown wakes a thread
 *      blocked in accept (shutdown(fd, SHUT_RDWR) does), addr returns the local port, close
 *      closes the socket. send and recv make no sense on a listener: return CHAN_CLOSED.
 *
 * Lecture 13 (sockets) and `man 7 tcp`, `man 2 accept4`, `man 3 getaddrinfo`, `man 7 socket`
 * (SO_SNDTIMEO, SO_REUSEADDR).
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>

chan_t *chan_tcp_open(const char *hostport, int mode) {
    (void)hostport;
    (void)mode;
    fprintf(stderr, "chan_tcp: not implemented (Phase 3, Task 1)\n");
    return NULL;
}

chan_t *chan_tcp_listen(const char *hostport) {
    (void)hostport;
    fprintf(stderr, "chan_tcp: not implemented (Phase 3, Task 1)\n");
    return NULL;
}
