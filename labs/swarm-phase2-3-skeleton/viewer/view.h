/* view.h — what a view is (given). A view draws Frames and turns user input into commands.
 *
 * The viewer program (viewer.c) owns the loop: it receives frames from a channel ("shm:NAME" for
 * a live show, "file:PATH" for a replay) and hands each one to the view; it asks the view for user
 * input and sends the commands to the center (live) or applies them to the replay. A view knows
 * nothing about channels, the center, captains or pipes: only Frames in, command lines out.
 * That is what lets you write your GUI (Task 2) without touching anything else.
 *
 * Commands a view may return (the same lines the operator tool sends, common/chan.h):
 *   PAUSE  RESUME  TEMPO <bpm>  KILL <leader_id>  STOP          (a live show: sent to the center)
 *   PAUSE  RESUME  TEMPO <bpm>  STEP  STOP                       (a replay: applied by the viewer)
 */
#ifndef VIEW_H
#define VIEW_H

#include <stddef.h>
#include "frame.h"

typedef struct {
    const char *out_path;        /* --out FILE (the ppm view)                                   */
    int         scale;           /* --scale PX: pixels per cell, for a window                   */
    int         live;            /* 1: a live show; 0: a replay                                 */
    int         argc;            /* the arguments after "--", for your own view's options       */
    char      **argv;
} ViewOptions;

typedef struct {
    const char *name;
    int  interactive;            /* 1: keeps running after the show ends, until the user quits  */
    int  (*open)(const ViewOptions *o);          /* 0 ok, -1 cannot (no display, ...)         */
    void (*draw)(const Frame *f);                /* show this frame; return quickly           */
    /* Wait at most wait_ms for user input. 1: cmd holds a command line; 0: nothing;
     * -1: the user wants to quit. Headless views just return 0. */
    int  (*input)(char *cmd, size_t n, int wait_ms);
    int  (*close)(void);                         /* the viewer's exit status                   */
} View;

extern const View view_term;     /* view_term.c:  live terminal view, keyboard controls (given) */
extern const View view_ascii;    /* view_ascii.c: the Phase 1 text frames, for render.py (given) */
extern const View view_ppm;      /* view_ppm.c:   the last frame as a PPM picture (given)        */
extern const View view_check;    /* view_check.c: checks every frame, for the tests (given)      */
extern const View view_gui;      /* view_gui.c:   YOUR window (Task 2)                           */

#endif
