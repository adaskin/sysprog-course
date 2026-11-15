/* planner.c — the stub choreographer (given). Deterministic for a given (theme, zone, seed). */
#include "planner.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static PlannerOpts opts;

void planner_init(const PlannerOpts *o) {
    opts = *o;
}

static unsigned hash32(unsigned x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void simulate_thinking(int cells) {
    if (opts.think_ms <= 0) {
        return;
    }
    long ms = (long)opts.think_ms * cells / 512;
    if (ms < opts.think_ms / 4) {
        ms = opts.think_ms / 4;
    }
    if (ms > (long)opts.think_ms * 4) {
        ms = (long)opts.think_ms * 4;
    }
    usleep((useconds_t)(ms * 1000));
}

/* ---- formations: every drone returns to its home cell at the end ------------- */

static void plan_formation(const Theme *t, const Zone *z, int n, unsigned seed, Keyframe *out) {
    int len = t->end_beat - t->start_beat + 1;
    int zw = z->x1 - z->x0 + 1;
    int rows = (n + zw - 1) / zw;
    int room = (z->y1 - (rows - 1)) - z->y0;                 /* free cells above the top home row */

    int hold = strcmp(t->theme, "hold") == 0;
    int dawn = strcmp(t->theme, "dawn") == 0;
    int calm = strcmp(t->mood, "calm") == 0;

    int amp = calm ? 2 : 4;
    int cap = dawn ? len / 4 : len / 6;                    /* keeps the speed limit with rounding */
    if (amp > cap) {
        amp = cap;
    }
    if (amp > room) {
        amp = room;
    }
    if (hold || amp < 1) {
        amp = 0;
    }
    double dir = (seed & 1) ? -1.0 : 1.0;                    /* style: mirror the wave */
    double phase0 = (double)(seed % 7) * 0.3;               /* style: shift the wave */

    for (int b = 0; b < len; b++) {
        double s = len > 1 ? (double)b / (double)(len - 1) : 0.0;
        for (int i = 0; i < n; i++) {
            int hx, hy;
            home_position(z, i, &hx, &hy);
            int col = hx - z->x0;
            double env = sin(M_PI * s);
            double wave = dawn ? 1.0 : sin(M_PI * s + dir * (0.5 * col) + phase0);
            int dy = amp ? -(int)lround(amp * env * wave) : 0;
            if (dy > 0) {
                dy = 0;                                      /* only ever go up from home */
            }
            Keyframe *k = &out[b * n + i];
            k->beat = t->start_beat + b;
            k->follower = i;
            k->x = hx;
            k->y = hy + dy;
            if (calm) {
                k->color = (unsigned char)(1 + (seed + (unsigned)col) % 15);
            } else {
                k->color = (unsigned char)(1 + (seed + (unsigned)col + (unsigned)b) % 15);
            }
        }
    }
}

/* ---- mosaic: drones fly to pixels of the target image ------------------------ */

typedef struct { int x, y; unsigned char c; } Target;

static int plan_mosaic(const Theme *t, const Zone *z, const Image *img, int n, Keyframe *out) {
    int len = t->end_beat - t->start_beat + 1;
    int zw = z->x1 - z->x0 + 1, zh = z->y1 - z->y0 + 1;

    /* pixels of the target inside this zone, sorted by (x, y) */
    Target *tg = malloc((size_t)zw * (size_t)zh * sizeof(Target));
    int *occ = malloc((size_t)zw * (size_t)zh * sizeof(int));
    if (!tg || !occ) {
        free(tg);
        free(occ);
        return -1;
    }
    int ntg = 0;
    for (int x = z->x0; x <= z->x1; x++) {
        for (int y = z->y0; y <= z->y1; y++) {
            if (x < img->w && y < img->h && img->px[y * img->w + x] != 0) {
                tg[ntg++] = (Target){x, y, img->px[y * img->w + x]};
            }
        }
    }

    /* drones in home order (by x, then y); the j-th drone takes the j-th (evenly spaced) pixel */
    int order[MAX_FOLLOWERS];
    Pos home[MAX_FOLLOWERS], goal[MAX_FOLLOWERS], pos[MAX_FOLLOWERS];
    unsigned char color[MAX_FOLLOWERS];
    for (int i = 0; i < n; i++) {
        home_position(z, i, &home[i].x, &home[i].y);
        order[i] = i;
    }
    for (int a = 1; a < n; a++) {                            /* insertion sort by (x, y) */
        int v = order[a], b = a - 1;
        while (b >= 0 && (home[order[b]].x > home[v].x ||
                          (home[order[b]].x == home[v].x && home[order[b]].y > home[v].y))) {
            order[b + 1] = order[b];
            b--;
        }
        order[b + 1] = v;
    }
    int used = ntg < n ? ntg : n;
    for (int i = 0; i < n; i++) {
        goal[i] = home[i];
        color[i] = 15;
    }
    for (int j = 0; j < used; j++) {
        Target *tt = &tg[(long)j * ntg / used];
        goal[order[j]] = (Pos){tt->x, tt->y};
        color[order[j]] = tt->c;
    }

    /* simulate beat by beat; drones move up to MAX_SPEED cells toward their goal into free cells */
    for (int i = 0; i < zw * zh; i++) {
        occ[i] = -1;
    }
    for (int i = 0; i < n; i++) {
        pos[i] = home[i];
        occ[(home[i].y - z->y0) * zw + (home[i].x - z->x0)] = i;
    }
    for (int b = 0; b < len; b++) {
        /* Drones are interchangeable: if drone k is sitting on the goal of drone i, they swap goals
         * (and colors), so k is done and i heads for k's old goal. This unblocks traffic jams. */
        for (int i = 0; i < n; i++) {
            if (goal[i].x == pos[i].x && goal[i].y == pos[i].y) {
                continue;
            }
            int k = occ[(goal[i].y - z->y0) * zw + (goal[i].x - z->x0)];
            if (k != -1 && k != i) {
                Pos g = goal[i];
                unsigned char c = color[i];
                goal[i] = goal[k];
                color[i] = color[k];
                goal[k] = g;
                color[k] = c;
            }
        }
        /* Farthest drones move first, so the drones that are already home do not wall them off. */
        int ord[MAX_FOLLOWERS], dist[MAX_FOLLOWERS];
        for (int i = 0; i < n; i++) {
            int ddx = abs(goal[i].x - pos[i].x), ddy = abs(goal[i].y - pos[i].y);
            dist[i] = ddx > ddy ? ddx : ddy;
            int a = i;
            while (a > 0 && dist[ord[a - 1]] < dist[i]) {
                ord[a] = ord[a - 1];
                a--;
            }
            ord[a] = i;
        }
        for (int oi = 0; oi < n; oi++) {
            int i = ord[oi];
            /* best free cell within MAX_SPEED (smallest squared distance to the goal); stay if none is closer */
            int own = (pos[i].y - z->y0) * zw + (pos[i].x - z->x0);
            long best = (long)(goal[i].x - pos[i].x) * (goal[i].x - pos[i].x) +
                        (long)(goal[i].y - pos[i].y) * (goal[i].y - pos[i].y);
            int bx = pos[i].x, by = pos[i].y;
            for (int ddy = -MAX_SPEED; ddy <= MAX_SPEED; ddy++) {
                for (int ddx = -MAX_SPEED; ddx <= MAX_SPEED; ddx++) {
                    int nx = pos[i].x + ddx, ny = pos[i].y + ddy;
                    if (nx < z->x0 || nx > z->x1 || ny < z->y0 || ny > z->y1) {
                        continue;
                    }
                    int cell = (ny - z->y0) * zw + (nx - z->x0);
                    if (occ[cell] != -1) {
                        continue;
                    }
                    long d = (long)(goal[i].x - nx) * (goal[i].x - nx) + (long)(goal[i].y - ny) * (goal[i].y - ny);
                    if (d < best) {
                        best = d;
                        bx = nx;
                        by = ny;
                    }
                }
            }
            occ[own] = -1;
            occ[(by - z->y0) * zw + (bx - z->x0)] = i;
            pos[i] = (Pos){bx, by};
        }
        for (int i = 0; i < n; i++) {
            Keyframe *k = &out[b * n + i];
            k->beat = t->start_beat + b;
            k->follower = i;
            k->x = pos[i].x;
            k->y = pos[i].y;
            k->color = color[i];
        }
    }
    free(tg);
    free(occ);
    return 0;
}

int plan_section(const Theme *theme, const Zone *zone, const Image *target,
                 int n_followers, unsigned style_seed, Keyframe *out, int max) {
    if (!theme || !zone || !out || n_followers < 1 || n_followers > MAX_FOLLOWERS) {
        return -1;
    }
    int len = theme->end_beat - theme->start_beat + 1;
    if (len < 1 || len > MAX_BEATS || zone->x1 < zone->x0 || zone->y1 < zone->y0) {
        return -1;
    }
    int zw = zone->x1 - zone->x0 + 1, zh = zone->y1 - zone->y0 + 1;
    int rows = (n_followers + zw - 1) / zw;
    int count = n_followers * len;
    if (count > max || rows > zh) {
        return -1;
    }

    simulate_thinking(count);

    if (strcmp(theme->theme, "mosaic") == 0) {
        if (!target || !target->px) {
            return -1;
        }
        if (plan_mosaic(theme, zone, target, n_followers, out) != 0) {
            return -1;
        }
    } else {
        plan_formation(theme, zone, n_followers, style_seed, out);
    }

    if (opts.flaky_pct > 0 && (int)(hash32(style_seed ^ 0x9e3779b9U) % 100) < opts.flaky_pct) {
        int b = len > 1 ? len / 2 : 0;                       /* teleport follower 0 five cells */
        out[b * n_followers].y += 5;
    }
    return count;
}
