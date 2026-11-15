/* frame_text.h — a Frame as text, for viewers over TCP (given, Phase 3).
 *
 * On the shared-memory bus and in a recording a Frame travels as it is, 4184 bytes. Over TCP it
 * travels as ONE message of text lines (project spec, Section 3), so a show can be watched with nc:
 *
 *   FRAME <beat> <last_beat> <w> <h> <tempo> <paused> <finished> <section> <section_name> <theme>
 *         <alive_mask> <collisions> <speed_violations> <busy> <late> <restarts> <n_leaders>
 *         <n_followers> <n>                                          (one line)
 *   D <x> <y> <color> <leader> <state>                               (n lines, one per drone)
 *
 * The viewer and swarmctl turn it back into a Frame, so every view works unchanged.
 */
#ifndef FRAME_TEXT_H
#define FRAME_TEXT_H

#include "chan.h"
#include "frame.h"

/* Send *f as one message on a line channel. 0, or a CHAN_ error. */
int frame_send_text(chan_t *c, const Frame *f);

/* Receive one frame: waits at most timeout_ms for its FRAME line. Returns 0 (and *f, sealed),
 * CHAN_TIMEOUT, CHAN_CLOSED, or -1 if the text was malformed (that frame is skipped). */
int frame_recv_text(chan_t *c, Frame *f, int timeout_ms);

#endif
