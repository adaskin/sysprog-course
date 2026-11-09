/* test_planner.c — the stub choreographer must produce valid plans (given). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "planner.h"
#include "validator.h"

static int failures;
#define CHECK_TRUE(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static Keyframe plan[MAX_FOLLOWERS * MAX_BEATS];

static int run(const char *theme, const char *mood, int start, int end, const Zone *z, int n,
               const Image *img, unsigned seed, int expect_valid) {
    Theme t = {0};
    snprintf(t.name, sizeof(t.name), "s");
    snprintf(t.theme, sizeof(t.theme), "%s", theme);
    snprintf(t.mood, sizeof(t.mood), "%s", mood);
    t.start_beat = start;
    t.end_beat = end;
    int cnt = plan_section(&t, z, img, n, seed, plan, (int)(sizeof(plan) / sizeof(plan[0])));
    if (cnt != n * (end - start + 1)) {
        printf("FAIL plan_section(%s,%s,n=%d) returned %d\n", theme, mood, n, cnt);
        failures++;
        return -1;
    }
    Pos home[MAX_FOLLOWERS];
    for (int i = 0; i < n; i++) {
        home_position(z, i, &home[i].x, &home[i].y);
    }
    PlanError err;
    int rc = validate_plan(&t, z, n, home, plan, cnt, &err);
    if ((rc == 0) != expect_valid) {
        printf("FAIL %s/%s n=%d beats %d..%d seed %u: %s (beat %d, follower %d)\n", theme, mood, n,
               start, end, seed, rc ? err.msg : "expected an invalid plan", err.beat, err.follower);
        failures++;
    }
    return rc;
}

int main(void) {
    PlannerOpts o = {0, 0};
    planner_init(&o);

    Image img;
    CHECK_TRUE(image_load("data/images/wave64x36.pgm", &img) == 0);

    static const char *themes[] = {"dawn", "waves", "hold", "sparkle"};
    static const char *moods[] = {"calm", "energetic", "triumphant"};
    Zone zones[4];
    for (int i = 0; i < 4; i++) {
        zone_split(64, 36, 4, i, &zones[i]);
    }
    int sizes[] = {1, 4, 16, 17, 40, 64};
    int lens[] = {1, 2, 8, 16, 32, 100};

    for (int z = 0; z < 4; z++)
        for (int s = 0; s < 6; s++)
            for (int th = 0; th < 4; th++)
                for (int m = 0; m < 3; m++)
                    for (int l = 0; l < 6; l++)
                        for (unsigned seed = 0; seed < 5; seed++) {
                            run(themes[th], moods[m], 10, 10 + lens[l] - 1, &zones[z], sizes[s], &img,
                                seed * 7919u + 13, 1);
                        }

    for (int z = 0; z < 4; z++)
        for (int s = 0; s < 6; s++)
            for (unsigned seed = 0; seed < 5; seed++) {
                run("mosaic", "triumphant", 32, 55, &zones[z], sizes[s], &img, seed * 31u + 1, 1);
            }

    /* the wave image gives every zone one pixel per column: with 16 drones in a 16-wide zone
     * they should all reach their pixel by the end of the 24-beat finale */
    {
        Theme t = {"finale", 32, 55, "mosaic", "triumphant"};
        Zone z = zones[1];
        plan_section(&t, &z, &img, 16, 1, plan, (int)(sizeof(plan) / sizeof(plan[0])));
        int hits = 0;
        for (int i = 0; i < 16; i++) {
            Keyframe *k = &plan[23 * 16 + i];
            if (img.px[k->y * img.w + k->x] == k->color) {
                hits++;
            }
        }
        CHECK_TRUE(hits == 16);
    }

    /* flaky mode: every plan must be rejected by the validator */
    o.flaky_pct = 100;
    planner_init(&o);
    for (unsigned seed = 0; seed < 20; seed++) {
        run("waves", "energetic", 0, 15, &zones[0], 16, &img, seed, 0);
        run("mosaic", "triumphant", 0, 23, &zones[0], 16, &img, seed, 0);
    }
    o.flaky_pct = 0;
    planner_init(&o);

    /* bad arguments */
    Theme t = {"x", 0, 3, "waves", "calm"};
    CHECK_TRUE(plan_section(&t, &zones[0], &img, 0, 1, plan, 100) == -1);
    CHECK_TRUE(plan_section(&t, &zones[0], &img, 16, 1, plan, 10) == -1);   /* max too small */

    image_free(&img);
    if (failures) {
        printf("test_planner: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_planner: all passed\n");
    return 0;
}
