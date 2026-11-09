/* validator.h — the plan checker (given). A captain MUST run every plan through this before
 * using it; a real model would sometimes get it wrong. */
#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "swarm.h"

typedef struct {
    int  beat, follower;
    char msg[96];
} PlanError;

/* Checks, in this order:
 *   - count == n_followers * (end - start + 1), every (beat, follower) exactly once
 *   - colors are in 0..NCOLORS-1
 *   - every position is inside the zone
 *   - speed: Chebyshev distance from the previous beat (or from `start` for the first beat) <= MAX_SPEED
 *   - no two followers in the same cell on the same beat
 * Returns 0 if the plan is valid, -1 otherwise (and fills *err if non-NULL). */
int validate_plan(const Theme *theme, const Zone *zone, int n_followers, const Pos *start,
                  const Keyframe *kf, int count, PlanError *err);

#endif
