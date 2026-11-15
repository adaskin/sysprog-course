/* frame.c — helpers for the view model (given). */
#include "frame.h"

#include <string.h>

_Static_assert(sizeof(DroneView) == 8, "DroneView must stay 8 bytes");
_Static_assert(sizeof(Frame) == 88 + 8 * FRAME_MAX_DRONES, "do not change the Frame layout");

uint32_t frame_checksum(const Frame *f) {
    const unsigned char *p = (const unsigned char *)f;
    size_t skip = offsetof(Frame, checksum);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(Frame); i++) {
        unsigned char c = (i >= skip && i < skip + sizeof(f->checksum)) ? 0 : p[i];
        h = (h ^ c) * 16777619u;
    }
    return h;
}

void frame_seal(Frame *f) {
    f->checksum = frame_checksum(f);
}

int frame_intact(const Frame *f) {
    return f->checksum == frame_checksum(f);
}

int frame_cells(const Frame *f, unsigned char *cells) {
    memset(cells, 0, (size_t)f->w * f->h);
    int n = 0;
    for (int i = 0; i < f->n_drones && i < FRAME_MAX_DRONES; i++) {
        const DroneView *d = &f->drones[i];
        if (d->x < f->w && d->y < f->h) {
            cells[d->y * f->w + d->x] = (unsigned char)(1 + (d->color & 15));
            n++;
        }
    }
    return n;
}
