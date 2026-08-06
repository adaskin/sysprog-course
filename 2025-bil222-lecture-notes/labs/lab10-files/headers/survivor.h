#ifndef SURVIVOR_H
#define SURVIVOR_H

#include "coord.h"
#include <time.h>
#include "list.h"
typedef struct survivor {
    int status;
    Coord coord;
    struct tm discovery_time;
    struct tm helped_time;
    char info[25];
} Survivor;

// Global survivor lists (extern)
extern List *survivors;          // Survivors awaiting help
extern List *helpedsurvivors;    // Helped survivors

// Functions
Survivor* create_survivor(Coord *coord, char *info, struct tm *discovery_time);
void *survivor_generator(void *args);

#endif