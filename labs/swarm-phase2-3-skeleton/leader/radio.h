/* radio.h — the captain's radio to its performers (Phase 3, given interface; radio.c is Task 3).
 *
 * In Phase 3 the followers are separate PROCESSES (bin/performer), started by their captain, and
 * they talk to it over UDP, one datagram per message (project spec, Section 3):
 *
 *   performer -> captain   NEXT <f> <beat>              I am ready for this beat (also: are you there?)
 *   captain -> performer   GOTO <f> <beat> <x> <y> <c>  be at (x, y) in color c on this beat
 *   performer -> captain   AT <f> <beat> <x> <y>        I am there
 *   captain -> performer   WAIT <f> <beat>              I am alive; that beat has not started yet
 *   captain -> performer   LAND <f>                     the show is over for you: exit
 *
 * Any datagram may be lost, duplicated or late. The captain keeps the follower THREADS of Phase 1:
 * a follower thread still decides the move and claims the cell (the grid lives in the captain);
 * then it calls radio_goto, which gets the performer process there.
 */
#ifndef RADIO_H
#define RADIO_H

/* Open the radio ("udp::0", any free port) and start the radio thread, which answers NEXT and
 * collects AT. Returns the port (the captain announces it in HELLO), or -1. */
int  radio_open(void);

/* After ASSIGN: there are nf performers, 0..nf-1. Unless `adopt`, start one bin/performer process
 * per follower:  performer --captain 127.0.0.1:<port> --center <center> --leader <id> --id <f>
 * With `adopt` (a restarted captain), start none: the performers of the dead captain will come
 * (ADOPT, then their NEXT). A follower that has no performer after about 2 s gets a new one. */
void radio_launch(int nf, int adopt, const char *center, int leader_id);

/* ADOPT <f> from the center: the next performer that says NEXT <f> takes slot f, even if
 * another performer had it (that one gets LAND when it talks again). */
void radio_adopt(int f);

/* Get performer f to (x, y) in color c for beat `beat`: send GOTO, resend it until the AT for this
 * beat arrives. Returns 0 (it is there) or -1 (no AT within ~3 s: the performer is late or lost;
 * the captain still reports the cell it claimed). Called by follower thread f; one call per beat. */
int  radio_goto(int f, int beat, int x, int y, int color);

/* The end: LAND every performer, wait (up to ~1 s) for the ones that are our children to exit
 * (SIGKILL the rest), stop the radio thread, close everything. */
void radio_close(void);

#endif
