/* metrics.c — audit and score (given). */
#include "metrics.h"
#include <stdlib.h>

int audit_collisions(const Pos *p, int n) {
    int c = 0;
    for (int i = 1; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (p[i].x == p[j].x && p[i].y == p[j].y) {
                c++;
                break;
            }
        }
    }
    return c;
}

int audit_speed(const Pos *prev, const Pos *cur, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) {
        int dx = abs(cur[i].x - prev[i].x), dy = abs(cur[i].y - prev[i].y);
        if ((dx > dy ? dx : dy) > MAX_SPEED) {
            c++;
        }
    }
    return c;
}

double mosaic_accuracy(const Image *img, const Pos *p, const unsigned char *color, int n) {
    if (n <= 0) {
        return 0.0;
    }
    int hit = 0;
    for (int i = 0; i < n; i++) {
        if (p[i].x >= 0 && p[i].x < img->w && p[i].y >= 0 && p[i].y < img->h &&
            color[i] != 0 && img->px[p[i].y * img->w + p[i].x] == color[i]) {
            hit++;
        }
    }
    return (double)hit / n;
}
