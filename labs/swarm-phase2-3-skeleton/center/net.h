/* net.h — the center's TCP server (Phase 3, given interface; net.c is Task 2).
 *
 * The center listens on ONE port. The first line of every connection says who is calling:
 *
 *   HELLO <id> <n> <udp_port>   a captain        -> center_attach_leader: the center takes the link
 *   WATCH                       a viewer         -> FRAME messages, newest frame wins, per viewer
 *   OPERATE                     an operator      -> command lines; each one is answered OK or ERR
 *   ORPHANED <f> <l> <beat>     an orphan        -> center_orphan answers REHOME or LAND, then close
 *
 * Anything else, or nothing within 2 s: close the connection.
 */
#ifndef NET_H
#define NET_H

#include "chan.h"
#include "frame.h"
#include "protocol.h"

/* ---- net.c (YOUR CODE, Task 2), called by center.c ---- */

/* Listen on addr ("tcp::PORT", 0 = any free port) and start accepting. 0, or -1 on error. */
int  net_start(const char *addr);
/* The port really listened on (useful with port 0), or -1. */
int  net_port(void);
/* Hand frame f to every connected viewer. Called by the clock thread after every beat: it must
 * NEVER block, whatever the viewers do (slow, stopped with SIGSTOP, gone). */
void net_publish(const Frame *f);
/* At the end of the show: stop accepting, let every viewer get the frames already published (the
 * final one!) for a short while, then close every connection and join every thread. */
void net_stop(void);

/* ---- center.c (given), called by net.c ---- */

/* A connection said HELLO: 0 if it became captain hello->leader_id's link (do not touch or close
 * it any more), -1 if refused (close it yourself). */
int  center_attach_leader(chan_t *link, const Msg *hello);
/* An operator's command line: 0 applied, -1 rejected. Answer the operator "OK" or "ERR ...". */
int  center_command(const char *line);
/* An orphan's ORPHANED message: answers it on c (REHOME or LAND). May wait a few seconds for a
 * captain to come back, so call it from the connection's own thread. Close c afterwards. */
void center_orphan(chan_t *c, const Msg *m);

#endif
