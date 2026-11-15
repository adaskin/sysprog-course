/* view_gui.c — the broadcast booth in a window, with SDL2: a BASIC version (given; Task 2: YOURS
 * to extend, or to replace with another toolkit).
 *
 *   make && bin/viewer --view gui                 (the Makefile finds SDL2 with pkg-config)
 *   bin/viewer --demo --view gui                  (a made-up show: no other task needs to work)
 *
 * What it does now: opens a window the size of the stage, draws one square per drone in its
 * color, writes the beat and the section in one status line, and has two keys: space (pause or
 * resume) and q / Esc / closing the window (quit the viewer, never the show). It implements the
 * four functions of view.h; view_term.c is a complete example of the same thing in a terminal.
 *
 * Task 2 (see the handout for the details and ideas), at least:
 *   - the stage: zone borders, the zone of a dead captain marked (its drones are already grey)
 *   - the status: tempo, paused, finished, captains alive, collisions, speed violations, restarts
 *   - controls (keys, buttons or both): tempo up/down, kill a captain (live), step (replay); a
 *     control only RETURNS a command line from input(): the viewer sends it
 *   - the window can be resized, and stays responsive during the whole show (also while paused)
 * This file includes only view.h, frame.h, palette.h, font5x7.h and the toolkit's headers: a view
 * knows nothing about channels, the center or the captains.
 */
#include <SDL.h>
#include <stdio.h>
#include "font5x7.h"
#include "palette.h"
#include "view.h"

#define STATUS_H 28                                  /* pixels below the stage for the status line */

static SDL_Window   *win;
static SDL_Renderer *ren;
static int scale, live, replay_paused, W, H;
static Frame last;
static int have_last;

static void set_color(Rgb c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
}

/* text at (x, y), each font pixel a px x px square; returns the x after the text */
static int draw_text(int x, int y, int px, Rgb c, const char *s) {
    set_color(c);
    for (; *s; s++, x += 6 * px) {
        const unsigned char *rows = font5x7_glyph(*s);
        for (int r = 0; rows && r < 7; r++) {
            for (int b = 0; b < 5; b++) {
                if (rows[r] >> (4 - b) & 1) {
                    SDL_Rect d = { x + b * px, y + r * px, px, px };
                    SDL_RenderFillRect(ren, &d);
                }
            }
        }
    }
    return x;
}

static void render(void) {
    set_color(PALETTE_BG);
    SDL_RenderClear(ren);
    if (!have_last) {
        draw_text(10, 10, 2, (Rgb){ 200, 200, 220 }, "WAITING FOR THE SHOW...");
        SDL_RenderPresent(ren);
        return;
    }
    const Frame *f = &last;
    for (int i = 0; i < f->n_drones && i < FRAME_MAX_DRONES; i++) {
        const DroneView *d = &f->drones[i];
        if (d->x >= f->w || d->y >= f->h) {
            continue;
        }
        set_color(d->state == DRONE_OK ? palette_color(d->color) : PALETTE_LOST);
        SDL_Rect r = { d->x * scale + 1, d->y * scale + 1, scale - 2, scale - 2 };
        SDL_RenderFillRect(ren, &r);
    }
    char line[96];
    snprintf(line, sizeof(line), "BEAT %d/%d  %.15s (%.15s)", f->beat, f->last_beat, f->section_name,
             f->theme);
    draw_text(8, f->h * scale + 7, 2, (Rgb){ 220, 222, 240 }, line);
    SDL_RenderPresent(ren);
}

static int gui_open(const ViewOptions *o) {
    live = o->live;
    scale = o->scale;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "viewer: SDL: %s (no display? try --view term)\n", SDL_GetError());
        return -1;
    }
    W = 64 * scale;                                  /* until the first frame tells the stage size */
    H = 36 * scale + STATUS_H;
    win = SDL_CreateWindow(live ? "swarm: live" : "swarm: replay", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, W, H, 0);
    ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
    if (!ren) {
        fprintf(stderr, "viewer: SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    render();
    return 0;
}

static void gui_draw(const Frame *f) {
    if (!have_last || f->w != last.w || f->h != last.h) {
        W = f->w * scale;                            /* the stage size of this show */
        H = f->h * scale + STATUS_H;
        SDL_SetWindowSize(win, W, H);
    }
    last = *f;
    have_last = 1;
    render();
}

/* Wait at most wait_ms for an event; turn keys into command lines (1), or quit (-1). */
static int gui_input(char *cmd, size_t n, int wait_ms) {
    SDL_Event e;
    int got = wait_ms > 0 ? SDL_WaitEventTimeout(&e, wait_ms) : SDL_PollEvent(&e);
    for (; got; got = SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return -1;
        }
        if (e.type == SDL_WINDOWEVENT) {
            render();                                /* uncovered or moved: paint again */
        } else if (e.type == SDL_KEYDOWN) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_q || k == SDLK_ESCAPE) {
                return -1;
            }
            if (k == SDLK_SPACE) {
                if (live) {                          /* the model knows whether it is paused */
                    snprintf(cmd, n, "%s", have_last && last.paused ? "RESUME" : "PAUSE");
                } else {
                    replay_paused = !replay_paused;  /* a replay is paused by the viewer itself */
                    snprintf(cmd, n, "%s", replay_paused ? "PAUSE" : "RESUME");
                }
                return 1;
            }
            /* Task 2: more keys (tempo, kill, step), mouse clicks, buttons ... */
        }
    }
    return 0;
}

static int gui_close(void) {
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

const View view_gui = { "gui", 1, gui_open, gui_draw, gui_input, gui_close };
