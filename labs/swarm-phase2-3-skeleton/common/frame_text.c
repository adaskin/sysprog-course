/* frame_text.c — a Frame as text lines and back (given, Phase 3). */
#define _GNU_SOURCE
#include "frame_text.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "swarm.h"

int frame_send_text(chan_t *c, const Frame *f) {
    int n = f->n_drones < FRAME_MAX_DRONES ? f->n_drones : FRAME_MAX_DRONES;
    size_t cap = 256 + (size_t)n * 32;
    char *buf = malloc(cap);
    if (!buf) {
        return CHAN_TOOBIG;
    }
    size_t o = (size_t)snprintf(buf, cap,
        "FRAME %d %d %u %u %u %u %u %u %s %s %u %u %u %u %u %u %u %u %d",
        f->beat, f->last_beat, f->w, f->h, f->tempo_bpm, f->paused, f->finished, f->section,
        f->section_name[0] ? f->section_name : "-", f->theme[0] ? f->theme : "-", f->alive_mask,
        f->collisions, f->speed_violations, f->busy, f->late, f->restarts, f->n_leaders,
        f->n_followers, n);
    for (int i = 0; i < n && o < cap; i++) {
        const DroneView *d = &f->drones[i];
        o += (size_t)snprintf(buf + o, cap - o, "\nD %u %u %u %u %u", d->x, d->y, d->color, d->leader,
                              d->state);
    }
    int r = o < cap ? chan_send(c, buf, o) : CHAN_TOOBIG;
    free(buf);
    return r;
}

/* the next number of a line; *ok = 0 if it is not one, or out of [lo, hi] */
static long num(char **save, long lo, long hi, int *ok) {
    char *t = strtok_r(NULL, " ", save), *end;
    if (!t) {
        *ok = 0;
        return 0;
    }
    errno = 0;
    long v = strtol(t, &end, 10);
    if (errno || *end || end == t || v < lo || v > hi) {
        *ok = 0;
    }
    return v;
}

static void word(char **save, char *out, size_t n, int *ok) {
    char *t = strtok_r(NULL, " ", save);
    if (!t || strlen(t) >= n) {
        *ok = 0;
        return;
    }
    snprintf(out, n, "%s", strcmp(t, "-") ? t : "");
}

int frame_recv_text(chan_t *c, Frame *f, int timeout_ms) {
    char line[MAX_LINE], *save = NULL;
    ssize_t r;
    do {                                             /* skip anything up to a FRAME line */
        r = chan_recv(c, line, sizeof(line), timeout_ms);
        if (r < 0 && r != CHAN_TOOBIG) {
            return (int)r;
        }
    } while (r < 0 || strncmp(line, "FRAME ", 6) != 0);
    memset(f, 0, sizeof(*f));
    int ok = 1, B = 1 << 30;
    strtok_r(line, " ", &save);
    f->version = FRAME_VERSION;
    f->beat = (int)num(&save, -1, B, &ok);
    f->last_beat = (int)num(&save, -1, B, &ok);
    f->w = (uint16_t)num(&save, 0, 255, &ok);
    f->h = (uint16_t)num(&save, 0, 255, &ok);
    f->tempo_bpm = (uint16_t)num(&save, 0, 65535, &ok);
    f->paused = (uint8_t)num(&save, 0, 1, &ok);
    f->finished = (uint8_t)num(&save, 0, 1, &ok);
    f->section = (uint8_t)num(&save, 0, 255, &ok);
    word(&save, f->section_name, sizeof(f->section_name), &ok);
    word(&save, f->theme, sizeof(f->theme), &ok);
    f->alive_mask = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->collisions = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->speed_violations = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->busy = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->late = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->restarts = (uint32_t)num(&save, 0, 0xFFFFFFFFL, &ok);
    f->n_leaders = (uint16_t)num(&save, 0, MAX_LEADERS, &ok);
    f->n_followers = (uint16_t)num(&save, 0, MAX_FOLLOWERS, &ok);
    int n = (int)num(&save, 0, FRAME_MAX_DRONES, &ok);
    if (!ok) {
        n = 0;                                       /* cannot know how many lines follow */
    }
    f->n_drones = (uint16_t)n;
    for (int i = 0; i < n; i++) {                    /* the D lines come in the same write */
        r = chan_recv(c, line, sizeof(line), 2000);
        if (r == CHAN_CLOSED || r == CHAN_TIMEOUT) {
            return (int)r;
        }
        save = NULL;
        char *t = r >= 0 ? strtok_r(line, " ", &save) : NULL;
        if (!t || strcmp(t, "D") != 0) {
            ok = 0;
            continue;
        }
        DroneView *d = &f->drones[i];
        d->x = (uint8_t)num(&save, 0, 255, &ok);
        d->y = (uint8_t)num(&save, 0, 255, &ok);
        d->color = (uint8_t)num(&save, 0, 15, &ok);
        d->leader = (uint8_t)num(&save, 0, MAX_LEADERS - 1, &ok);
        d->state = (uint8_t)num(&save, 0, 255, &ok);
    }
    if (!ok) {
        return -1;
    }
    frame_seal(f);
    return 0;
}
