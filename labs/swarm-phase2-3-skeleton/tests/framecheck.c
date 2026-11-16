/* framecheck.c — audit a recording made by any center (given).
 *
 *   framecheck FILE [-v]
 *
 * Reads the file directly (PROTOCOL.md 1.4: a 4-byte little-endian length, then the bytes; here
 * every message must be a Frame) and checks, independently of the center that wrote it:
 *   - every frame is whole (magic and FNV-1a check, PROTOCOL.md 6.1);
 *   - beats never go back, and none is missing between the first and the last frame;
 *   - no two drones (DRONE_OK or DRONE_LOST) share a cell, in any beat;
 *   - no drone moves more than MAX_SPEED cells per beat, or leaves the stage. A drone that has not
 *     moved for k beats may move 2k cells: a late captain's drones are shown where they were last
 *     known, and catch up;
 *   - the center's own totals (collisions, speed_violations) agree with what is recorded;
 *   - the last frame, and only it, is marked finished.
 * Prints one FRAMECHECK line and exits 0 when everything holds, 1 otherwise.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "swarm.h"

static uint32_t fnv(const Frame *f) {
    Frame copy = *f;
    copy.check = 0;
    const unsigned char *p = (const unsigned char *)&copy;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(copy); i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static int present(const DroneView *d) {
    return d->state == DRONE_OK || d->state == DRONE_LOST;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: framecheck FILE [-v]\n");
        return 2;
    }
    int verbose = argc > 2 && !strcmp(argv[2], "-v");
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror(argv[1]);
        return 2;
    }
    Frame *f = malloc(sizeof(Frame)), *prev = calloc(1, sizeof(Frame));
    int *moved_at = calloc(MAX_DRONES, sizeof(int));     /* beat of each drone's last change */
    long frames = 0, beats = 0, torn = 0, gaps = 0, back = 0, bad = 0, collisions = 0, speed = 0;
    long finished = 0, after_finish = 0, bad_state = 0;
    uint32_t center_coll = 0, center_speed = 0;
    int have_prev = 0, first_beat = 0, last_beat = -1;
    static unsigned char seen[MAX_STAGE + 1][MAX_STAGE + 1];

    for (;;) {
        unsigned char len4[4];
        size_t got = fread(len4, 1, 4, fp);
        if (got == 0) {
            break;
        }
        uint32_t len = (uint32_t)len4[0] | (uint32_t)len4[1] << 8 | (uint32_t)len4[2] << 16 |
                       (uint32_t)len4[3] << 24;
        if (got < 4 || len != sizeof(Frame) || fread(f, 1, sizeof(Frame), fp) != sizeof(Frame)) {
            if (got == 4 && len != sizeof(Frame)) {
                fprintf(stderr, "frame %ld: a message of %u bytes, not a Frame (%zu)\n", frames, len,
                        sizeof(Frame));
                bad++;
            }
            break;                                       /* cut short: the end */
        }
        frames++;
        if (finished) {
            after_finish++;
        }
        if (f->magic != FRAME_MAGIC || f->check != fnv(f)) {
            torn++;
            if (verbose || torn <= 3) {
                fprintf(stderr, "frame %ld: torn (magic or check does not match)\n", frames - 1);
            }
            continue;
        }
        int n = f->n_captains * f->n_followers;
        if (n > MAX_DRONES || f->w > MAX_STAGE + 1 || f->h > MAX_STAGE + 1 ||
            (have_prev && (f->w != prev->w || f->h != prev->h || n != prev->n_captains * prev->n_followers))) {
            fprintf(stderr, "frame %ld: stage or drone count changed or out of range\n", frames - 1);
            bad++;
            continue;
        }
        if (f->finished) {
            finished++;
        }
        center_coll = f->collisions;
        center_speed = f->speed_violations;
        if (have_prev && f->beat < prev->beat) {
            back++;
            fprintf(stderr, "frame %ld: beat %d after beat %d\n", frames - 1, f->beat, prev->beat);
            continue;
        }
        if (have_prev && f->beat == prev->beat) {
            *prev = *f;                                  /* a repeat (paused): nothing moves */
            continue;
        }
        if (have_prev && f->beat > prev->beat + 1 && prev->beat >= 0) {
            gaps += f->beat - prev->beat - 1;
            fprintf(stderr, "frame %ld: beats %d..%d are missing\n", frames - 1, prev->beat + 1, f->beat - 1);
        }
        if (f->beat >= 0) {
            beats++;
            if (beats == 1) {
                first_beat = f->beat;
            }
            last_beat = f->beat;
        }
        memset(seen, 0, sizeof(seen));
        for (int i = 0; i < n; i++) {
            const DroneView *d = &f->drones[i];
            if (d->state > DRONE_LOST) {
                bad_state++;
                continue;
            }
            if (!present(d)) {
                continue;
            }
            if (d->x >= f->w || d->y >= f->h) {
                speed++;
                if (verbose || speed <= 5) {
                    fprintf(stderr, "beat %d: drone %d is off the stage at (%u,%u)\n", f->beat, i, d->x, d->y);
                }
            } else if (++seen[d->x][d->y] == 2) {
                collisions++;
                if (verbose || collisions <= 5) {
                    fprintf(stderr, "beat %d: collision at (%u,%u)\n", f->beat, d->x, d->y);
                }
            }
            if (!have_prev) {
                moved_at[i] = f->beat;
                continue;
            }
            const DroneView *p = &prev->drones[i];
            if (!present(p)) {
                moved_at[i] = f->beat;                   /* it appeared: a new performer */
                continue;
            }
            if (p->x != d->x || p->y != d->y) {
                int dt = f->beat - moved_at[i];
                int dx = abs((int)d->x - (int)p->x), dy = abs((int)d->y - (int)p->y);
                if (dt < 1 || dx > MAX_SPEED * dt || dy > MAX_SPEED * dt) {
                    speed++;
                    if (verbose || speed <= 5) {
                        fprintf(stderr, "beat %d: drone %d jumped (%u,%u) -> (%u,%u) in %d beats\n", f->beat,
                                i, p->x, p->y, d->x, d->y, dt);
                    }
                }
                moved_at[i] = f->beat;
            }
        }
        *prev = *f;
        have_prev = 1;
    }
    fclose(fp);

    int counts_agree = center_coll == (uint32_t)collisions && center_speed == (uint32_t)speed;
    if (!counts_agree) {
        fprintf(stderr, "the center counted %u collisions and %u speed violations; the recording shows %ld and %ld\n",
                center_coll, center_speed, collisions, speed);
    }
    if (frames > 0 && !prev->finished) {
        fprintf(stderr, "the last frame is not marked finished\n");
    }
    printf("FRAMECHECK frames=%ld beats=%ld first=%d last=%d torn=%ld gaps=%ld collisions=%ld speed=%ld "
           "finished=%ld center_collisions=%u center_speed=%u\n",
           frames, beats, first_beat, last_beat, torn, gaps, collisions, speed, finished, center_coll,
           center_speed);
    int ok = frames > 0 && torn == 0 && gaps == 0 && back == 0 && bad == 0 && bad_state == 0 &&
             collisions == 0 && speed == 0 && finished == 1 && after_finish == 0 && prev->finished &&
             counts_agree;
    free(f);
    free(prev);
    free(moved_at);
    return ok ? 0 : 1;
}
