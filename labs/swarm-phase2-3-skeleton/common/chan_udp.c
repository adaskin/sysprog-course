/* chan_udp.c — the udp: transport, the radio between a captain and its performers
 * (Phase 3, Task 1: YOUR CODE).
 *
 * One datagram is one message: no line framing, no '\n', no partial reads. But a datagram may be
 * lost, duplicated or arrive out of order, and nobody tells you. The radio protocol (NEXT, GOTO,
 * AT, ...) copes with that; this file only moves datagrams.
 *
 *   chan_open("udp::PORT", CHAN_BOTH)       the SERVER (the captain): bind to PORT on every
 *                                           interface (0 = any free port; chan_addr tells which).
 *                                           It receives from anybody; chan_send answers the sender
 *                                           of the last message received.
 *   chan_open("udp:HOST:PORT", CHAN_BOTH)   a CLIENT (a performer): connect() the socket to the
 *                                           server, then plain send/recv.
 *   chan_peer(server)                       a NEW send-only channel to the sender of the last
 *                                           message, sharing the server's socket. The captain keeps
 *                                           one per performer, so it can push GOTO to any of them
 *                                           at any time. Closing it must not close the socket.
 *
 * Rules (tests/test_chan udp checks them):
 *
 *   1. A message is at most 512 bytes: a longer chan_send returns CHAN_TOOBIG. In chan_recv, a
 *      datagram longer than the buffer also gives CHAN_TOOBIG (recvfrom with MSG_TRUNC returns
 *      the real length), and the next datagram is fine. Add a '\0' after the message when it fits.
 *   2. chan_recv waits with poll() up to timeout_ms (-1 = forever) and returns CHAN_TIMEOUT.
 *      The server remembers where each message came from (recvfrom fills a sockaddr_storage).
 *   3. Sending to a port where nobody listens: Linux reports the ICMP "port unreachable" on a
 *      connected socket as ECONNREFUSED on a later send or recv. Return CHAN_CLOSED for it (a
 *      performer uses it to notice a dead captain sooner). Every other send error is a lost
 *      datagram like any other: return 0.
 *   4. The server answers with sendto(last sender); a peer channel with sendto(its address); a
 *      client with send(). Two threads may send on two peer channels of one socket at once: a
 *      single sendto is atomic, so that is fine.
 *   5. chan_shutdown wakes a thread blocked in chan_recv (CHAN_CLOSED) within its timeout. UDP has
 *      no connection to shut down: a flag that recv checks around poll() is enough, as long as the
 *      callers use a finite timeout.
 *   6. Close-on-exec (SOCK_CLOEXEC), as in tcp.
 *   7. Simulated radio interference, for the tests: if the environment variable SWARM_UDP_LOSS is
 *      set to a percentage, chan_send silently drops that share of the messages (returns 0 without
 *      sending). Use rand_r with a per-channel seed, so threads do not share rand() state.
 *      Example: SWARM_UDP_LOSS=30 bin/center ... must still finish the show.
 *
 * Lecture 13 (sockets), `man 7 udp`, `man 2 recvfrom` (MSG_TRUNC), `man 3 getaddrinfo`.
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>

chan_t *chan_udp_open(const char *hostport, int mode) {
    (void)hostport;
    (void)mode;
    fprintf(stderr, "chan_udp: not implemented (Phase 3, Task 1)\n");
    return NULL;
}
