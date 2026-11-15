/* view_ppm.c — writes the last frame as a picture, without a display (given).
 *
 * At the end it writes --out FILE (default frame.ppm) as a binary PPM ("P6"): a header line and
 * then 3 bytes per pixel. Any image viewer opens it; `convert frame.ppm frame.png` makes a PNG.
 * The tests use it to check a run on a machine without a screen.
 */
#include <stdio.h>
#include "palette.h"
#include "view.h"

static Frame last;
static int have_last, scale = 8;
static const char *out_path;

static int ppm_open(const ViewOptions *o) {
    out_path = o->out_path;
    scale = o->scale > 8 ? 8 : o->scale;
    return 0;
}

static void ppm_draw(const Frame *f) {
    last = *f;
    have_last = 1;
}

static int ppm_input(char *cmd, size_t n, int wait_ms) {
    (void)cmd;
    (void)n;
    (void)wait_ms;
    return 0;
}

static int ppm_close(void) {
    if (!have_last) {
        fprintf(stderr, "viewer: no frame received, %s not written\n", out_path);
        return 1;
    }
    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        perror(out_path);
        return 1;
    }
    static signed short who[256 * 256];
    for (int i = 0; i < last.w * last.h; i++) {
        who[i] = -1;
    }
    for (int i = 0; i < last.n_drones && i < FRAME_MAX_DRONES; i++) {
        if (last.drones[i].x < last.w && last.drones[i].y < last.h) {
            who[last.drones[i].y * last.w + last.drones[i].x] = (signed short)i;
        }
    }
    fprintf(fp, "P6\n%d %d\n255\n", last.w * scale, last.h * scale);
    for (int py = 0; py < last.h * scale; py++) {
        for (int px = 0; px < last.w * scale; px++) {
            int x = px / scale, y = py / scale, i = who[y * last.w + x];
            int dx = px % scale, dy = py % scale;
            int inside = dx > 0 && dy > 0 && dx < scale - 1 && dy < scale - 1;   /* a small gap */
            Rgb c = PALETTE_BG;
            if (i >= 0 && inside) {
                c = last.drones[i].state == DRONE_OK ? palette_color(last.drones[i].color)
                                                     : PALETTE_LOST;
            }
            fputc(c.r, fp);
            fputc(c.g, fp);
            fputc(c.b, fp);
        }
    }
    fclose(fp);
    return 0;
}

const View view_ppm = { "ppm", 0, ppm_open, ppm_draw, ppm_input, ppm_close };
