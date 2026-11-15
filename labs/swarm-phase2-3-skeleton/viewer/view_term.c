/* view_term.c — a live view in the terminal, with keyboard controls (given).
 *
 * It is a complete example of a view: draw() paints the stage with 24-bit ANSI colors (each
 * character cell shows two stage cells, one above the other, with the "▀" half block), and
 * input() turns keys into command lines. Your GUI (view_gui.c) does the same with a window.
 *
 *   space  pause / resume        +  -  tempo         1..8  kill captain 0..7
 *   s      one step (replay)     x     stop the show q     quit the viewer
 */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "palette.h"
#include "view.h"

static struct termios saved;
static int raw_mode, live, replay_paused;
static Frame last;
static int have_last;

static void restore_terminal(void) {
    if (raw_mode) {
        tcsetattr(0, TCSANOW, &saved);
        raw_mode = 0;
    }
    printf("\x1b[0m\x1b[?25h\n");                    /* reset colors, show the cursor */
    fflush(stdout);
}

static int term_open(const ViewOptions *o) {
    live = o->live;
    if (isatty(0) && tcgetattr(0, &saved) == 0) {
        struct termios t = saved;
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);     /* keys arrive at once, without echo */
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        if (tcsetattr(0, TCSANOW, &t) == 0) {
            raw_mode = 1;
        }
    }
    atexit(restore_terminal);
    printf("\x1b[2J\x1b[?25l");                      /* clear the screen, hide the cursor */
    return 0;
}

static Rgb cell_color(const Frame *f, const signed short *who, int x, int y) {
    if (y >= f->h) {
        return PALETTE_BG;
    }
    int i = who[y * f->w + x];
    if (i < 0) {
        int zone_w = f->n_leaders ? (f->w + f->n_leaders - 1) / f->n_leaders : f->w;
        return (zone_w && x % zone_w == 0 && x) ? PALETTE_ZONE : PALETTE_BG;
    }
    if (f->drones[i].state != DRONE_OK) {
        return PALETTE_LOST;
    }
    return palette_color(f->drones[i].color);
}

static void term_draw(const Frame *f) {
    last = *f;
    have_last = 1;
    static signed short who[256 * 256];
    if (f->w == 0 || f->h == 0) {
        return;
    }
    for (int i = 0; i < f->w * f->h; i++) {
        who[i] = -1;
    }
    for (int i = 0; i < f->n_drones && i < FRAME_MAX_DRONES; i++) {
        if (f->drones[i].x < f->w && f->drones[i].y < f->h) {
            who[f->drones[i].y * f->w + f->drones[i].x] = (signed short)i;
        }
    }
    printf("\x1b[H");                                /* cursor home: redraw in place */
    for (int y = 0; y < f->h; y += 2) {
        for (int x = 0; x < f->w; x++) {
            Rgb top = cell_color(f, who, x, y), bot = cell_color(f, who, x, y + 1);
            printf("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm\xe2\x96\x80",
                   top.r, top.g, top.b, bot.r, bot.g, bot.b);
        }
        printf("\x1b[0m\n");
    }
    char alive[16] = {0};
    for (int l = 0; l < f->n_leaders && l < 15; l++) {
        alive[l] = (f->alive_mask >> l & 1) ? (char)('0' + l) : 'x';
    }
    printf("\x1b[K beat %d/%d  %s (%s)  tempo %d  captains %s%s%s\n", f->beat, f->last_beat,
           f->section_name, f->theme, f->tempo_bpm, alive, f->paused ? "  [PAUSED]" : "",
           f->finished ? "  [END]" : "");
    printf("\x1b[K collisions %u  speed violations %u  busy %u  late %u  restarts %u\n",
           f->collisions, f->speed_violations, f->busy, f->late, f->restarts);
    printf("\x1b[K %s\n", live ? "space pause/resume  + - tempo  1-8 kill captain  x stop show  q quit"
                               : "space pause/resume  s step  + - speed  q quit   (replay)");
    fflush(stdout);
}

static int term_input(char *cmd, size_t n, int wait_ms) {
    if (!raw_mode) {                                 /* not a terminal: no keys */
        if (wait_ms > 0) {
            poll(NULL, 0, wait_ms);
        }
        return 0;
    }
    struct pollfd p = { .fd = 0, .events = POLLIN };
    if (poll(&p, 1, wait_ms) <= 0) {
        return 0;
    }
    char c;
    if (read(0, &c, 1) != 1) {
        return 0;
    }
    int tempo = have_last ? last.tempo_bpm : 120;
    switch (c) {
    case ' ':
        if (!live) {
            replay_paused = !replay_paused;          /* a replay keeps its own pause state */
            snprintf(cmd, n, "%s", replay_paused ? "PAUSE" : "RESUME");
        } else {
            snprintf(cmd, n, "%s", have_last && last.paused ? "RESUME" : "PAUSE");
        }
        return 1;
    case '+': snprintf(cmd, n, "TEMPO %d", tempo + 20 > 2000 ? 2000 : tempo + 20); return 1;
    case '-': snprintf(cmd, n, "TEMPO %d", tempo > 20 ? tempo - 20 : 0); return 1;
    case 's': snprintf(cmd, n, "STEP"); return 1;
    case 'x': snprintf(cmd, n, "STOP"); return 1;
    case 'q': return -1;
    default:
        if (c >= '1' && c <= '8') {
            snprintf(cmd, n, "KILL %d", c - '1');
            return 1;
        }
        return 0;
    }
}

static int term_close(void) {
    restore_terminal();
    return 0;
}

const View view_term = { "term", 1, term_open, term_draw, term_input, term_close };
