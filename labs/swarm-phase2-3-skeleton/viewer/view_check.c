/* view_check.c — checks every frame it receives and prints a summary (given; used by the tests).
 *
 *   CHECK frames=58 torn=0 order_errors=0 last_beat=55 finished=1 lost_seen=0 paused_seen=0 ...
 *
 * torn:          frames whose checksum is wrong (half old, half new: Task 1 rule 2)
 * order_errors:  a beat older than the one before (frames must never go back in time)
 * Exit status 0 if at least one frame arrived, none was torn and none went back in time.
 */
#include <stdio.h>
#include "view.h"

static long frames, torn, order_errors, lost_seen, paused_seen, version_errors;
static int last_beat = -1000, finished;
static Frame last;

static int check_open(const ViewOptions *o) {
    (void)o;
    return 0;
}

static void check_draw(const Frame *f) {
    frames++;
    if (!frame_intact(f)) {
        torn++;
        return;
    }
    if (f->version != FRAME_VERSION) {
        version_errors++;
        return;
    }
    if (f->beat < last_beat) {
        order_errors++;
    }
    last_beat = f->beat;
    finished |= f->finished;
    paused_seen += f->paused;
    for (int i = 0; i < f->n_drones && i < FRAME_MAX_DRONES; i++) {
        if (f->drones[i].state == DRONE_LOST) {
            lost_seen++;
            break;
        }
    }
    last = *f;
}

static int check_input(char *cmd, size_t n, int wait_ms) {
    (void)cmd;
    (void)n;
    (void)wait_ms;
    return 0;
}

static int check_close(void) {
    printf("CHECK frames=%ld torn=%ld order_errors=%ld version_errors=%ld last_beat=%d finished=%d "
           "lost_seen=%ld paused_seen=%ld restarts=%u collisions=%u speed_violations=%u\n",
           frames, torn, order_errors, version_errors, last_beat, finished, lost_seen, paused_seen,
           last.restarts, last.collisions, last.speed_violations);
    fflush(stdout);
    return (frames > 0 && torn == 0 && order_errors == 0 && version_errors == 0) ? 0 : 1;
}

const View view_check = { "check", 0, check_open, check_draw, check_input, check_close };
