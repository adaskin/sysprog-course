/* validator.c — checks a plan against the safety rules (given). */
#include "validator.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(PlanError *e, int beat, int follower, const char *what) {
    if (e) {
        e->beat = beat;
        e->follower = follower;
        snprintf(e->msg, sizeof(e->msg), "%s", what);
    }
    return -1;
}

static int cheb(int ax, int ay, int bx, int by) {
    int dx = abs(ax - bx), dy = abs(ay - by);
    return dx > dy ? dx : dy;
}

int validate_plan(const Theme *t, const Zone *z, int n, const Pos *start,
                  const Keyframe *kf, int count, PlanError *err) {
    int len = t->end_beat - t->start_beat + 1;
    if (n < 1 || n > MAX_FOLLOWERS || len < 1 || len > MAX_BEATS) {
        return fail(err, -1, -1, "bad arguments");
    }
    if (count != n * len) {
        return fail(err, -1, -1, "wrong number of keyframes");
    }
    Pos *tab = malloc((size_t)count * sizeof(Pos));
    if (!tab) {
        return fail(err, -1, -1, "out of memory");
    }
    for (int i = 0; i < count; i++) {
        tab[i].x = INT_MIN;                                  /* "not seen yet" */
    }
    int rc = 0;
    for (int i = 0; i < count && rc == 0; i++) {
        const Keyframe *k = &kf[i];
        if (k->beat < t->start_beat || k->beat > t->end_beat || k->follower < 0 || k->follower >= n) {
            rc = fail(err, k->beat, k->follower, "beat or follower out of range");
        } else if (tab[(k->beat - t->start_beat) * n + k->follower].x != INT_MIN) {
            rc = fail(err, k->beat, k->follower, "duplicate keyframe");
        } else if (k->color >= NCOLORS) {
            rc = fail(err, k->beat, k->follower, "bad color");
        } else if (!zone_contains(z, k->x, k->y)) {
            rc = fail(err, k->beat, k->follower, "outside the zone");
        } else {
            tab[(k->beat - t->start_beat) * n + k->follower] = (Pos){k->x, k->y};
        }
    }
    for (int b = 0; b < len && rc == 0; b++) {
        for (int f = 0; f < n && rc == 0; f++) {
            Pos cur = tab[b * n + f];
            Pos prev = b == 0 ? start[f] : tab[(b - 1) * n + f];
            if (cheb(cur.x, cur.y, prev.x, prev.y) > MAX_SPEED) {
                rc = fail(err, t->start_beat + b, f, "moved faster than the speed limit");
            }
            for (int g = 0; g < f && rc == 0; g++) {
                if (tab[b * n + g].x == cur.x && tab[b * n + g].y == cur.y) {
                    rc = fail(err, t->start_beat + b, f, "two followers in the same cell");
                }
            }
        }
    }
    free(tab);
    return rc;
}
