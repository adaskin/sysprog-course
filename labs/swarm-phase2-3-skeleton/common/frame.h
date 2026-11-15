/* frame.h — the view model (given).
 *
 * A Frame is the whole stage after one beat, as the center audited it. It is the ONLY thing a
 * viewer ever sees of the show: viewers know nothing about captains, plans or pipes. The center
 * builds one Frame per beat and publishes it (stage bus, flight recorder; in Phase 3 also TCP).
 *
 * The struct has a fixed size and no pointers, so it can be copied with memcpy into shared
 * memory, written to a file, and read back by another program. Do not reorder or resize the
 * fields: recordings made by one build must be readable by another (see FRAME_VERSION).
 */
#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>
#include "swarm.h"

#define FRAME_VERSION     1
#define FRAME_MAX_DRONES  (MAX_LEADERS * MAX_FOLLOWERS)       /* 512 */

/* DroneView.state */
enum {
    DRONE_OK     = 0,     /* flying, its captain is alive                                  */
    DRONE_LOST   = 1,     /* its captain is dead (Phase 2: waiting for the restart)        */
    DRONE_ORPHAN = 2      /* Phase 3: a performer process whose captain died               */
};

typedef struct {
    uint8_t x, y;         /* cell on the stage                                              */
    uint8_t color;        /* 0..15 (0 = dark)                                               */
    uint8_t leader;       /* id of its captain                                              */
    uint8_t state;        /* DRONE_OK, DRONE_LOST, ...                                      */
    uint8_t pad[3];
} DroneView;              /* 8 bytes */

typedef struct {
    uint32_t  version;            /* FRAME_VERSION                                          */
    int32_t   beat;               /* this frame shows the stage after this beat             */
    int32_t   last_beat;          /* the show ends after this beat                          */
    uint16_t  w, h;               /* stage size in cells                                    */
    uint16_t  n_leaders, n_followers;
    uint16_t  n_drones;           /* n_leaders * n_followers; drone i belongs to captain
                                     i / n_followers                                        */
    uint16_t  tempo_bpm;          /* current tempo (0 = as fast as possible)                */
    uint8_t   paused;             /* 1 while the operator has paused the clock              */
    uint8_t   finished;           /* 1 in the last frame of the show                        */
    uint8_t   section;            /* index of the current section in the score              */
    uint8_t   pad0;
    char      section_name[16];
    char      theme[16];
    uint32_t  alive_mask;         /* bit l set: captain l is alive                          */
    uint32_t  collisions;         /* totals since the start of the show                     */
    uint32_t  speed_violations;
    uint32_t  busy;
    uint32_t  late;
    uint32_t  restarts;
    uint32_t  checksum;           /* frame_checksum(): lets a reader detect a torn copy     */
    DroneView drones[FRAME_MAX_DRONES];
} Frame;

/* FNV-1a over the whole frame, with the checksum field treated as 0. */
uint32_t frame_checksum(const Frame *f);
/* Set f->checksum. The center seals every frame before it publishes it. */
void     frame_seal(Frame *f);
/* 1 if the checksum matches: the frame was not torn (half old, half new) by a racing writer. */
int      frame_intact(const Frame *f);

/* Draw the frame into cells[w*h]: 0 for an empty cell, otherwise 1 + the drone's color
 * (so a dark drone, color 0, is still visible as 1). Returns the number of drones drawn. */
int      frame_cells(const Frame *f, unsigned char *cells);

#endif
