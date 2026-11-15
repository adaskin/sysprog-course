/* view_ascii.c — the stage as text, one block per beat (given).
 *
 * The same format as the Phase 1 center's --ascii: "--- beat N ---" and then one line per row,
 * '.' for an empty cell and a hex digit (the color) for a drone. tools/render.py turns it into
 * PNG and GIF pictures (see show.sh), and text is easy to compare in a test.
 */
#include <stdio.h>
#include "view.h"

static int printed_beat = -2;

static int ascii_open(const ViewOptions *o) {
    (void)o;
    return 0;
}

static void ascii_draw(const Frame *f) {
    if (f->beat == printed_beat) {                   /* a pause publishes the same beat again */
        return;
    }
    printed_beat = f->beat;
    static unsigned char cells[256 * 256];
    if ((size_t)f->w * f->h > sizeof(cells)) {
        return;
    }
    frame_cells(f, cells);
    printf("--- beat %d ---\n", f->beat);
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            unsigned char c = cells[y * f->w + x];
            putchar(c ? "0123456789abcdef"[(c - 1) & 15] : '.');
        }
        putchar('\n');
    }
    fflush(stdout);
}

static int ascii_input(char *cmd, size_t n, int wait_ms) {
    (void)cmd;
    (void)n;
    (void)wait_ms;
    return 0;
}

static int ascii_close(void) {
    return 0;
}

const View view_ascii = { "ascii", 0, ascii_open, ascii_draw, ascii_input, ascii_close };
