/* viewer.c — the broadcast booth: watches a live show or replays a recording (given).
 *
 *   ./viewer                          watch the live show of this user (shm:swarm-<uid>)
 *   ./viewer --name NAME              watch the live show NAME
 *   ./viewer --replay show.rec        replay a recording (file:show.rec)
 *   ./viewer --demo                   a made-up show inside the viewer: start your GUI (Task 2)
 *                                     before the stage bus works
 *   ./viewer --center HOST:PORT       Phase 3: watch over TCP (WATCH; FRAME messages), from any
 *                                     machine; commands go back as OPERATE
 *   --view term|gui|ascii|ppm|check   how to show it (default: term)
 *
 * The viewer is a separate program. It links only common/ (frames, channels) and viewer/: it does
 * not even see the center's or the captains' code. It receives Frames from a channel and gives
 * them to a view (view.h); user input comes back from the view as command lines, which go to the
 * center through the control channel (live) or steer the replay (recording). Nothing a viewer
 * does can slow the show down: the stage bus never waits for a receiver (Task 1).
 * Over TCP (Phase 3) the frames arrive as text (frame_text.h) and become the same Frame: the
 * views cannot tell the difference.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "chan.h"
#include "frame.h"
#include "frame_text.h"
#include "swarm.h"
#include "view.h"

static const View *views[] = { &view_term, &view_gui, &view_ascii, &view_ppm, &view_check };

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

static void usage(void) {
    fprintf(stderr,
            "usage: viewer [--name NAME | --center HOST:PORT | --replay FILE | --demo]\n"
            "              [--view term|gui|ascii|ppm|check]\n"
            "              [--out FILE] [--scale PX] [--wait SEC] [--delay-ms MS] [-- view options]\n");
}

/* ------------------------------------------------- --demo: a made-up show, no center needed */

#define DEMO_BEATS 240
static int demo, demo_dead;                          /* demo_dead: bit l = captain l is "dead" */

static void demo_frame(Frame *f, int beat, int tempo, int paused) {
    memset(f, 0, sizeof(*f));
    f->version = FRAME_VERSION;
    f->beat = beat;
    f->last_beat = DEMO_BEATS - 1;
    f->w = 64;
    f->h = 36;
    f->n_leaders = 4;
    f->n_followers = 46;
    f->n_drones = 4 * 46;
    f->tempo_bpm = (uint16_t)tempo;
    f->paused = (uint8_t)paused;
    f->finished = beat == DEMO_BEATS - 1;
    f->section = (uint8_t)(beat / 60);
    snprintf(f->section_name, sizeof(f->section_name), "demo%d", beat / 60 + 1);
    snprintf(f->theme, sizeof(f->theme), "%s", (const char *[]){ "waves", "spiral", "rain", "waves" }[beat / 60 % 4]);
    f->alive_mask = 0xFu & ~(unsigned)demo_dead;
    for (int i = 0; i < f->n_drones; i++) {
        int l = i / 46, k = i % 46;
        double t = beat / 8.0, x = l * 16 + k % 16, y = 18 + 12 * sin(t + x / 6.0) * cos(k / 16 * 0.9);
        DroneView *d = &f->drones[i];
        d->x = (uint8_t)x;
        d->y = (uint8_t)(y < 0 ? 0 : y > 35 ? 35 : y);
        d->color = (uint8_t)(1 + (l * 16 + k % 16) * 14 / 64);
        d->leader = (uint8_t)l;
        d->state = (demo_dead >> l & 1) ? DRONE_LOST : DRONE_OK;
    }
    frame_seal(f);
}

/* ---------------------------------------------------------------- commands from the view */

static const char *show_name;
static const char *center;                           /* --center HOST:PORT (Phase 3) */
static int live;

/* Phase 3: one operator command over TCP: OPERATE, the command, the center's OK or ERR. */
static int operate(const char *cmd) {
    char addr[300], reply[MAX_LINE];
    snprintf(addr, sizeof(addr), "tcp:%s", center);
    chan_t *c = chan_open(addr, CHAN_BOTH);
    int ok = c && chan_send(c, "OPERATE", 7) == 0 && chan_send(c, cmd, strlen(cmd)) == 0 &&
             chan_recv(c, reply, sizeof(reply), 2000) >= 2 && !strncmp(reply, "OK", 2);
    chan_close(c);
    return ok ? 0 : -1;
}
static int replay_paused, replay_step, replay_tempo = -1, quit;

/* Live: the command goes to the center. Replay: the viewer applies it. */
static void handle_command(const char *cmd) {
    int id;
    if (demo && sscanf(cmd, "KILL %d", &id) == 1 && id >= 0 && id < 4) {
        demo_dead ^= 1 << id;                        /* the demo "restarts" it on the next KILL */
        return;
    }
    if (live && !demo && center) {
        if (operate(cmd) != 0) {
            fprintf(stderr, "viewer: the center at %s did not take '%s'\n", center, cmd);
        }
        return;
    }
    if (live && !demo) {
        char addr[128];
        ctl_addr(show_name, addr, sizeof(addr));
        chan_t *c = chan_open(addr, CHAN_SEND);
        if (!c || chan_send(c, cmd, strlen(cmd)) != 0) {
            fprintf(stderr, "viewer: cannot send '%s': no show '%s' is listening\n", cmd, show_name);
        }
        chan_close(c);
        return;
    }
    int bpm;
    char extra;
    if (!strcmp(cmd, "PAUSE")) {
        replay_paused = 1;
    } else if (!strcmp(cmd, "RESUME")) {
        replay_paused = 0;
    } else if (!strcmp(cmd, "STEP")) {
        replay_step = 1;
    } else if (!strcmp(cmd, "STOP")) {
        quit = 1;
    } else if (sscanf(cmd, "TEMPO %d %c", &bpm, &extra) == 1 && bpm >= 0 && bpm <= 2000) {
        replay_tempo = bpm;
    }                                                /* KILL and the rest: not for a replay */
}

/* ------------------------------------------------------------------------------- main */

int main(int argc, char **argv) {
    static char default_name[32];
    snprintf(default_name, sizeof(default_name), "swarm-%u", (unsigned)getuid());
    show_name = default_name;
    const char *replay = NULL, *view_name = "term";
    ViewOptions vo = { .out_path = "frame.ppm", .scale = 16 };
    int wait_s = 0, delay_ms = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int has = i + 1 < argc;
        if (!strcmp(a, "--name") && has)            show_name = argv[++i];
        else if (!strcmp(a, "--center") && has)     center = argv[++i];
        else if (!strcmp(a, "--replay") && has)     replay = argv[++i];
        else if (!strcmp(a, "--demo"))              demo = 1;
        else if (!strcmp(a, "--view") && has)       view_name = argv[++i];
        else if (!strcmp(a, "--out") && has)        vo.out_path = argv[++i];
        else if (!strcmp(a, "--scale") && has)      vo.scale = atoi(argv[++i]);
        else if (!strcmp(a, "--wait") && has)       wait_s = atoi(argv[++i]);
        else if (!strcmp(a, "--delay-ms") && has)   delay_ms = atoi(argv[++i]);   /* a slow viewer */
        else if (!strcmp(a, "--")) {
            vo.argc = argc - i - 1;
            vo.argv = argv + i + 1;
            break;
        } else {
            usage();
            return 2;
        }
    }
    const View *view = NULL;
    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
        if (!strcmp(views[i]->name, view_name)) {
            view = views[i];
        }
    }
    if (!view || vo.scale < 1 || vo.scale > 64 || wait_s < 0 || delay_ms < 0) {
        usage();
        return 2;
    }
    live = replay == NULL;                           /* the demo plays a live show */
    vo.live = live;
    signal(SIGPIPE, SIG_IGN);                        /* a center that goes away is an error, not death */

    /* the source: one channel, live or recorded: the rest of the program does not care */
    char addr[512];
    int tcp = live && center;                        /* Phase 3: frames as text over TCP */
    if (tcp) {
        snprintf(addr, sizeof(addr), "tcp:%s", center);
    } else if (live) {
        snprintf(addr, sizeof(addr), "shm:%s", show_name);
    } else {
        snprintf(addr, sizeof(addr), "file:%s", replay);
    }
    int mode = tcp ? CHAN_BOTH : CHAN_RECV;
    chan_t *src = demo ? NULL : chan_open(addr, mode);
    for (long t = now_ms(); !src && !demo && live && now_ms() - t < wait_s * 1000L; ) {
        sleep_ms(100);                               /* --wait: the show has not started yet */
        src = chan_open(addr, mode);
    }
    if (src && tcp && chan_send(src, "WATCH", 5) != 0) {
        chan_close(src);
        src = NULL;
    }
    if (!src && !demo) {
        fprintf(stderr, "viewer: cannot open %s%s\n", addr,
                live ? " (is the center running? same --name?)" : "");
        return 1;
    }
    if (view->open(&vo) != 0) {
        chan_close(src);
        return 1;
    }

    /* the loop: frames in, commands out */
    static Frame f;
    int src_done = 0;
    long next_due = 0;                               /* replay: when the next frame is due */
    int demo_beat = 0;
    while (!quit) {
        int can_read = !src_done && ((live && !demo) || ((!replay_paused || replay_step) && now_ms() >= next_due));
        if (demo && !src_done && (replay_paused && !replay_step) && now_ms() >= next_due) {
            demo_frame(&f, demo_beat - 1, replay_tempo >= 0 ? replay_tempo : 240, 1);   /* paused */
            view->draw(&f);
            next_due = now_ms() + 250;
        } else if (demo && can_read) {
            int bpm = replay_tempo >= 0 ? replay_tempo : 240;
            demo_frame(&f, demo_beat++, bpm, 0);
            view->draw(&f);
            replay_step = 0;
            next_due = view->interactive && bpm > 0 ? now_ms() + 60000 / bpm : 0;
            src_done = demo_beat == DEMO_BEATS;
            if (src_done && !view->interactive) {
                break;
            }
        } else if (can_read) {
            ssize_t r;
            if (tcp) {                               /* the same Frame, from its text form */
                r = frame_recv_text(src, &f, 30);
                static Frame g;                      /* TCP queues frames in the socket buffers: */
                while (r == 0 && frame_recv_text(src, &g, 0) == 0) {
                    f = g;                           /* a slow viewer draws only the newest one */
                }
                r = r == 0 ? (ssize_t)sizeof(f) : r;
            } else {
                r = chan_recv(src, &f, sizeof(f), live ? 30 : 0);
            }
            if (r == (ssize_t)sizeof(f)) {
                view->draw(&f);
                replay_step = 0;
                if (!live && view->interactive) {   /* replay at the recorded (or chosen) tempo */
                    int bpm = replay_tempo >= 0 ? replay_tempo : f.tempo_bpm;
                    next_due = now_ms() + (bpm > 0 ? 60000 / bpm : 0);
                }
                if (delay_ms) {
                    sleep_ms(delay_ms);
                }
            } else if (r == CHAN_CLOSED) {
                src_done = 1;
                if (!view->interactive) {
                    break;
                }
            } else if (r == CHAN_TIMEOUT) {
                if (!live) {
                    next_due = now_ms() + 50;        /* a recording still being written: look again soon */
                }
            } else {
                fprintf(stderr, "viewer: unexpected message (%zd bytes) ignored\n", r);
            }
        }
        int wait = 0;                                /* how long the view may wait for input */
        if (src_done || ((!live || demo) && replay_paused && !replay_step)) {
            wait = 100;
        } else if (!live || demo) {
            long left = next_due - now_ms();
            wait = left > 0 ? (left < 100 ? (int)left : 100) : 0;
        }
        char cmd[MAX_LINE];
        int q;
        while ((q = view->input(cmd, sizeof(cmd), wait)) == 1) {
            handle_command(cmd);
            wait = 0;
        }
        if (q < 0) {
            break;
        }
        if (!view->interactive && wait > 0) {
            sleep_ms(wait);                          /* headless views do not wait for input: we do */
        }
    }
    int status = view->close();
    chan_close(src);                                 /* NULL (the demo) is fine */
    return status;
}
