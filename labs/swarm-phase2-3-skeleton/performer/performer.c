/* performer.c — one drone, as its own process (Phase 3, Task 3: YOUR CODE).
 *
 *   performer --captain HOST:PORT --center HOST:PORT --leader L --id F
 *
 * Its captain starts it (radio.c). It talks to the captain over UDP, one datagram per message
 * (the radio protocol is in leader/radio.h and in the project spec, Section 3), and to the center
 * over TCP only when its captain is gone. Suggested shape:
 *
 *   radio = chan_open("udp:<captain>", CHAN_BOTH); say NEXT F <last_beat + 1> (last_beat starts at -1)
 *   loop:
 *     wait for a datagram, at most ~250 ms
 *     GOTO F b x y c   with b >= last_beat: "fly" there (remember it), last_beat = b, answer AT F b x y.
 *                      (b == last_beat is a resend: our AT was lost; answer again.) Older: ignore.
 *     WAIT F b         the captain is alive, the beat has not started: nothing to do
 *     LAND F           exit 0
 *     nothing for 250 ms: say NEXT again (it asks for a lost GOTO, and proves we are alive)
 *     nothing at all from the captain for ~1.5 s, or a send/recv says CHAN_CLOSED (Linux reports
 *     the ICMP "port unreachable" of a dead captain as ECONNREFUSED: chan_udp.c rule 3):
 *         we are an ORPHAN: rescue
 *
 *   rescue: connect to the center (chan_open "tcp:<center>"), say ORPHANED F L <last_beat>, wait
 *     for ONE answer (the center may take a few seconds: wait up to ~8 s), close the connection.
 *       REHOME F host port   a new captain: reopen the radio to "host:port" (write "[host]:port" if
 *                            host is IPv6) and say NEXT. If it is the SAME captain as before (a
 *                            false alarm: datagrams were lost), keep the socket you have.
 *       LAND F               exit 0
 *       no answer, or the center cannot be reached: try again with randomized exponential
 *       backoff (100 ms, 200 ms, ... plus a random part: why random?); after ~6 tries, land on
 *       your own (exit 0). No performer may outlive the show for long.
 *
 * Ignore datagrams for another F and anything malformed. Ignore SIGPIPE. Log to stderr with
 * log_msg (swarm.h), and not much: there are many performers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include "chan.h"
#include "protocol.h"
#include "swarm.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "performer: not implemented (Phase 3, Task 3)\n");
    return 1;
}
