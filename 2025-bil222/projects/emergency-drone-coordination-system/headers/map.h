#ifndef MAP_H
#define MAP_H

#include "survivor.h"
#include "list.h"
#include "coord.h"

typedef struct mapcell {
    Coord coord;
    List *survivors;    // Survivors in this cell
} MapCell;

typedef struct map {
    int height, width;
    MapCell **cells;
} Map;

// Global map instance (extern)
extern Map map;

// Functions
void init_map(int height, int width);
void freemap();

#endif