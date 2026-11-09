/* swarm.h — shared types for the Drone Swarm Choreographers project (given). */
#ifndef SWARM_H
#define SWARM_H

#include <stddef.h>

#define MAX_LINE       256      /* longest protocol line, including the newline */
#define MAX_LEADERS    8
#define MAX_FOLLOWERS  64       /* per leader */
#define MAX_SECTIONS   16
#define MAX_BEATS      256      /* longest section, in beats */
#define MAX_SPEED      2        /* cells per beat (Chebyshev distance) */
#define NCOLORS        16       /* colors are 0..15; 0 = dark */

typedef struct { int x0, y0, x1, y1; } Zone;       /* inclusive corners */
typedef struct { int x, y; } Pos;

/* "At `beat`, performer `follower` is at (x, y) showing `color`." */
typedef struct {
    int           beat;
    int           follower;
    int           x, y;
    unsigned char color;
} Keyframe;

/* One section of the music. The center sends one of these as a THEME message. */
typedef struct {
    char name[16];
    int  start_beat, end_beat;      /* inclusive */
    char theme[16];                 /* dawn, waves, hold, mosaic, ... */
    char mood[16];                  /* calm, energetic, triumphant, ... */
} Theme;

typedef struct { int w, h; unsigned char *px; } Image;  /* px[y*w + x], 0 = empty */

typedef struct {
    int   stage_w, stage_h;
    int   tempo_bpm;
    char  image_path[128];
    Theme sections[MAX_SECTIONS];
    int   n_sections;
} Score;

/* ---- score and image files ---------------------------------------------- */
int  score_load(const char *path, Score *s);        /* 0 ok, -1 error (message on stderr) */
int  image_load(const char *path, Image *img);      /* ASCII PGM (P2), values 0..15 */
void image_free(Image *img);

/* ---- helpers ------------------------------------------------------------ */
/* Zone i of n: the stage is cut into n vertical strips of (nearly) equal width. */
void zone_split(int stage_w, int stage_h, int n, int i, Zone *z);
int  zone_contains(const Zone *z, int x, int y);

/* Home formation: follower i starts at the bottom of its zone, filling rows from the
 * bottom up, left to right. Every formation section starts and ends here. */
void home_position(const Zone *z, int i, int *x, int *y);

/* The section that contains `beat`, or -1. */
int  score_section_at(const Score *s, int beat);
int  score_last_beat(const Score *s);

/* printf-style logging to stderr with a prefix like "[leader 2] ". */
void log_msg(const char *who, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif
