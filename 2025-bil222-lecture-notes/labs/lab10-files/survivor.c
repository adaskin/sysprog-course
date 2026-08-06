#include "headers/survivor.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "headers/globals.h"
#include "headers/map.h"

Survivor *create_survivor(Coord *coord, char *info,
                          struct tm *discovery_time) {
    Survivor *s = malloc(sizeof(Survivor));
    if (!s) return NULL;

    memset(s, 0, sizeof(Survivor));
    s->coord = *coord;
    memcpy(&s->discovery_time, discovery_time, sizeof(struct tm));
    strncpy(s->info, info, sizeof(s->info) - 1);
    s->info[sizeof(s->info) - 1] = '\0';  // Ensure null-termination
    s->status = 0;  // Initialize status (e.g., 0 for waiting)
    return s;
}

void *survivor_generator(void *args) {
    (void)args;  // Unused parameter
    time_t t;
    struct tm discovery_time;

    while (1) {
        // Generate random survivor
        Coord coord = {.x = rand() % map.width,
                       .y = rand() % map.height};

        char info[25];
        snprintf(info, sizeof(info), "SURV-%04d", rand() % 10000);

        time(&t);
        localtime_r(&t, &discovery_time);

        // Create and add to lists
        Survivor *s = create_survivor(&coord, info, &discovery_time);
        if (!s) continue;

        // Add to global survivor list
        pthread_mutex_lock(&survivors->lock);
        survivors->add(survivors, s);
        pthread_mutex_unlock(&survivors->lock);

        // Add to map cell's survivor list
        pthread_mutex_lock(
            &map.cells[coord.x][coord.y].survivors->lock);
        map.cells[coord.x][coord.y].survivors->add(
            map.cells[coord.x][coord.y].survivors, s);
        pthread_mutex_unlock(
            &map.cells[coord.x][coord.y].survivors->lock);

        printf("New survivor at (%d,%d): %s\n", coord.x, coord.y,
               info);
        sleep(rand() % 3 + 2);  // Generate every 2-5 seconds
    }
    return NULL;
}

void survivor_cleanup(Survivor *s) {
    // Remove from map cell
    pthread_mutex_lock(
        &map.cells[s->coord.x][s->coord.y].survivors->lock);
    map.cells[s->coord.x][s->coord.y].survivors->removedata(
        map.cells[s->coord.x][s->coord.y].survivors, s);
    pthread_mutex_unlock(
        &map.cells[s->coord.x][s->coord.y].survivors->lock);

    free(s);
}