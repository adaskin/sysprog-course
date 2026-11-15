/* metrics.h — the center's audit and score (given). */
#ifndef METRICS_H
#define METRICS_H

#include "swarm.h"

/* Number of drones that share a cell with a lower-numbered drone (0 = no collisions). */
int    audit_collisions(const Pos *p, int n);

/* Number of drones that moved more than MAX_SPEED cells since the previous beat. */
int    audit_speed(const Pos *prev, const Pos *cur, int n);

/* Fraction (0..1) of the n drones that sit on a non-empty pixel of the target whose value equals
 * the drone's color. */
double mosaic_accuracy(const Image *target, const Pos *p, const unsigned char *color, int n);

#endif
