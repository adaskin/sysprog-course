/* planner.h — the choreographer (given). The center calls plan_section on behalf of a leader.
 * In a real show this would be a slow, sometimes wrong LLM; here it is a stub with simulated
 * latency. THREAD-SAFE: it only reads its options and writes to `out`.
 *
 * Contract of a plan: DENSE and in beat-major order. For every beat in
 * [theme->start_beat, theme->end_beat] and every follower 0..n_followers-1 there is exactly
 * one keyframe, so count == n_followers * (end - start + 1).
 *
 * Every formation theme (dawn, waves, hold, ...) starts and ends at the home formation
 * (home_position), so formation sections chain. The theme "mosaic" starts at home and moves the
 * drones onto pixels of the target image; it must be the LAST section.
 *
 * Sections fed to a leader that is restarted mid-show start at the resume beat, so the plan
 * always begins at home. */
#ifndef PLANNER_H
#define PLANNER_H

#include "swarm.h"

typedef struct {
    int think_ms;     /* base simulated thinking time per call (0 = instant) */
    int flaky_pct;    /* 0..100: percent of plans that contain a deliberate safety violation */
} PlannerOpts;

void planner_init(const PlannerOpts *opts);

/* Returns the number of keyframes written to out (== n_followers * beats), or -1 if `max` is
 * too small or the arguments are invalid. Sleeps for the simulated thinking time. */
int plan_section(const Theme *theme, const Zone *zone, const Image *target,
                 int n_followers, unsigned style_seed, Keyframe *out, int max);

#endif
