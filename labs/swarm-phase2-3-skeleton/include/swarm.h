/* swarm.h — the shared numbers and the one shared binary layout (given: no code).
 *
 * Everything else that programs exchange is text, defined in PROTOCOL.md. The Frame below is the
 * only binary message: the center writes one per beat to the stage bus (shm:) and to the
 * recording (file:), and every viewer and the tests read it back. Its layout is fixed so that a
 * recording made by any team can be checked by tests/framecheck.
 */
#ifndef SWARM_H
#define SWARM_H

#include <stdint.h>

/* ---- limits ------------------------------------------------------------------------------- */
#define MAX_LINE        256     /* longest protocol line, including the '\n'                   */
#define MAX_CAPTAINS    8
#define MAX_FOLLOWERS   64      /* drones per captain                                          */
#define MAX_DRONES      (MAX_CAPTAINS * MAX_FOLLOWERS)
#define MAX_SECTIONS    16
#define MAX_STAGE       255     /* the stage is at most 255 x 255 cells                        */
#define MAX_SPEED       2       /* cells per beat, in each of x and y (Chebyshev distance)      */
#define NCOLORS         16      /* colors 0..15; 0 is dark                                      */

/* ---- timing (milliseconds), PROTOCOL.md Section 4 ----------------------------------------- */
#define HEARTBEAT_MS        200   /* a captain says HEARTBEAT this often                        */
#define CAPTAIN_DEAD_MS    1000   /* the center: a captain silent this long is dead             */
#define RADIO_RETRY_MS       50   /* a performer repeats NEXT this often while it waits          */
#define ORPHAN_MS          1500   /* a performer: no answer from its captain this long: orphaned */
#define PERFORMER_LOST_MS  3000   /* a captain: nothing from slot f this long: LOST <f>          */

/* ---- the frame: the stage after one beat --------------------------------------------------- */
#define FRAME_MAGIC 0x31465753u   /* "SWF1" in memory on a little-endian machine */

enum {                      /* DroneView.state */
    DRONE_NONE = 0,         /* no drone in this slot (not drawn)                                */
    DRONE_OK   = 1,         /* flying                                                           */
    DRONE_LOST = 2          /* its captain is dead or its performer is lost: hovering in place  */
};

typedef struct {
    uint8_t x, y;           /* its cell                                                         */
    uint8_t color;          /* 0..15                                                            */
    uint8_t state;          /* DRONE_NONE, DRONE_OK, DRONE_LOST                                 */
} DroneView;

typedef struct {
    uint32_t  magic;        /* FRAME_MAGIC                                                       */
    uint32_t  check;        /* FNV-1a of the whole frame with check = 0 (PROTOCOL.md 6.1): lets a
                               reader detect a torn copy, half old and half new                  */
    int32_t   beat;         /* the stage after this beat                                         */
    int32_t   last_beat;    /* the show ends after this beat                                     */
    uint16_t  w, h;         /* stage size in cells                                               */
    uint16_t  n_captains;   /* zones in the show                                                 */
    uint16_t  n_followers;  /* slots per zone: drone i belongs to captain i / n_followers        */
    uint16_t  tempo_bpm;    /* 0: as fast as possible                                            */
    uint8_t   paused;       /* 1 while an operator has paused the show                           */
    uint8_t   finished;     /* 1 in the last frame of the show                                   */
    uint32_t  collisions;   /* totals since the start of the show                                */
    uint32_t  speed_violations;
    DroneView drones[MAX_DRONES];   /* n_captains * n_followers are used                        */
} Frame;

_Static_assert(sizeof(Frame) == 36 + 4 * MAX_DRONES, "the Frame layout is fixed: do not change it");

#endif
