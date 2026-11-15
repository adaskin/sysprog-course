/* net.c — the center's TCP server (Phase 3, Task 2: YOUR CODE). The interface is in net.h.
 *
 * One port, four kinds of callers, told apart by the first line of the connection (net.h):
 * captains (HELLO), viewers (WATCH), operators (OPERATE) and orphaned performers (ORPHANED).
 *
 * Choose your concurrency model and justify it in REPORT.md:
 *   (a) a thread per connection: an accept thread (chan_accept with a timeout of ~200 ms, so it
 *       notices net_stop) starts a thread per connection; that thread reads the first line (wait
 *       at most 2 s: a caller that says nothing must not hold anything up) and then serves it;
 *   (b) one poll() loop over every socket (you need the descriptors: add an operation to the
 *       channel API, or keep your own sockets), with non-blocking sends and per-viewer buffers.
 * (a) is simpler with blocking channels and fits the rest of the center; (b) scales to thousands
 * of connections. Either is fine here.
 *
 * What each kind needs:
 *   HELLO     center_attach_leader(c, &msg). 0: the center owns the channel now (it starts its
 *             own reader thread, as with the pipes): forget it, do not close it. -1: close it.
 *   WATCH     the Phase 2 stage-bus rules, per connection: every viewer has a slot that holds
 *             only the NEWEST frame; net_publish overwrites the slots (and wakes the senders) and
 *             never touches a socket, so it cannot block; each viewer has its own sender (a
 *             thread, or your poll loop) that sends the newest frame it has not sent yet with
 *             frame_send_text. A slow viewer skips frames; a stopped one (SIGSTOP) blocks only its
 *             own sender, until the send times out (SO_SNDTIMEO in chan_tcp.c): then drop it.
 *             A viewer that connects late gets the newest frame at once.
 *   OPERATE   command lines until the operator closes: center_command(line), then answer "OK"
 *             or "ERR <reason>" (one line each). The command may be STOP.
 *   ORPHANED  center_orphan(c, &msg) answers it (it may wait a few seconds), then close.
 *
 * net_stop (the end of the show): stop accepting; give viewers a moment (up to 1 s) to send the
 * frames already published, the final one included; then chan_shutdown whatever is still open
 * (it wakes threads blocked in recv or send), join every thread, close every channel, free
 * everything. valgrind and `lsof -p <center>` must find nothing left.
 *
 * Tests: tests/run_tests.sh net (a show over TCP with viewers and operators; slow and frozen
 * viewers). Try it by hand with nc:  nc localhost 9000, then type WATCH.
 */
#define _GNU_SOURCE
#include "net.h"

#include <stdio.h>
#include "frame_text.h"

int net_start(const char *addr) {
    (void)addr;
    fprintf(stderr, "net: not implemented (Phase 3, Task 2)\n");
    return -1;
}

int net_port(void) {
    return -1;
}

void net_publish(const Frame *f) {
    (void)f;
}

void net_stop(void) {
}
