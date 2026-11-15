/* radio.c — the captain's radio to its performer processes (Phase 3, Task 3: YOUR CODE).
 * The interface and the radio protocol are in radio.h; read it first.
 *
 * A suggested design (you may choose another one; explain yours in REPORT.md):
 *
 *   one radio channel:  chan_open("udp::0", CHAN_BOTH): the server; chan_addr(radio, CHAN_LOCAL)
 *                       gives the port
 *   one radio thread:   chan_recv(radio, ..., 100 ms) in a loop until radio_close; it answers NEXT
 *                       and collects AT
 *   one slot per follower, all protected by ONE mutex (every operation under it is short):
 *       peer            chan_peer(radio) for the performer of this slot: GOTO, WAIT and LAND go there
 *       host, port      the performer's address (chan_addr(radio, CHAN_REMOTE, ...) right after the
 *                       recv): only datagrams from this address count for this slot
 *       pid             the performer process we started for it (0: none)
 *       open            ADOPT (or the performer seems lost): the next NEXT for this slot takes it
 *       goto_beat, x, y, color      the target of the current beat (radio_goto sets it)
 *       at_beat         the last beat the performer confirmed with AT
 *   a condition variable: radio_goto waits on it (timed, ~40 ms) and resends GOTO each time it wakes
 *   up without the AT; the radio thread broadcasts when an AT arrives
 *
 *   the radio thread, for each datagram (parse it with proto_parse; ignore anything malformed):
 *     NEXT f b   from the slot's performer, or for a slot that has none or is open: (re)take the
 *                slot (a new chan_peer); from anybody else: answer LAND f (a duplicate: a spare
 *                and a rehomed orphan both think they are f).
 *                Then answer GOTO (the current target) if goto_beat >= b, otherwise WAIT f b.
 *     AT f b     from the slot's performer, for the current goto_beat: at_beat = b, broadcast.
 *   and every time round the loop: reap performers that exited (waitpid(-1, ..., WNOHANG)), and
 *   in --adopt mode, ~2 s after radio_launch, start a new performer for every slot that still
 *   has nobody (an orphan that never came back).
 *
 *   starting a performer: fork, then execv bin/performer (next to bin/leader: readlink
 *   /proc/self/exe, as center.c does for bin/leader) with --captain 127.0.0.1:<port> --center
 *   <center> --leader <id> --id <f>. The child must not touch the mutex or call printf: build the
 *   argument strings BEFORE fork, and between fork and exec call only async-signal-safe functions.
 *
 *   radio_close: send LAND to every performer (3 times: datagrams get lost), stop and join the
 *   radio thread, close the peers and the radio, then wait for your children (~1 s, then SIGKILL).
 *   A performer that missed every LAND gets ECONNREFUSED on its next NEXT, asks the center and is
 *   told LAND: no performer outlives the show.
 *
 * Tests: tests/run_tests.sh radio (a show with performer processes; SWARM_UDP_LOSS=30; a captain
 * killed in the middle: its performers are adopted by the next one).
 */
#define _GNU_SOURCE
#include "radio.h"

#include <stdio.h>
#include "chan.h"
#include "protocol.h"
#include "swarm.h"

int radio_open(void) {
    fprintf(stderr, "radio: not implemented (Phase 3, Task 3)\n");
    return -1;
}

void radio_launch(int nf, int adopt, const char *center, int leader_id) {
    (void)nf;
    (void)adopt;
    (void)center;
    (void)leader_id;
}

void radio_adopt(int f) {
    (void)f;
}

int radio_goto(int f, int beat, int x, int y, int color) {
    (void)f;
    (void)beat;
    (void)x;
    (void)y;
    (void)color;
    return -1;
}

void radio_close(void) {
}
