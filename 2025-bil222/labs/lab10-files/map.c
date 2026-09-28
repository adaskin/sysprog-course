#include "headers/map.h"
#include "headers/list.h"
#include <stdlib.h>
#include <stdio.h>

// Global map instance (defined here, declared extern in map.h)
Map map;

void init_map(int height, int width) {
    map.height = height;
    map.width = width;

    // Allocate rows (height)
    map.cells = (MapCell**)malloc(sizeof(MapCell*) * height);
    if (!map.cells) {
        perror("Failed to allocate map rows");
        exit(EXIT_FAILURE);
    }

    // Allocate columns (width) for each row
    for (int i = 0; i < height; i++) {
        map.cells[i] = (MapCell*)malloc(sizeof(MapCell) * width);
        if (!map.cells[i]) {
            perror("Failed to allocate map columns");
            exit(EXIT_FAILURE);
        }

        // Initialize each cell
        for (int j = 0; j < width; j++) {
            map.cells[i][j].coord.x = i;
            map.cells[i][j].coord.y = j;
            // Create a survivor list for this cell (capacity 10)
            map.cells[i][j].survivors = create_list(sizeof(Survivor), 10);
        }
    }

    printf("Map initialized: %dx%d\n", height, width);
}

void freemap() {
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            // Destroy the survivor list in each cell
            map.cells[i][j].survivors->destroy(map.cells[i][j].survivors);
        }
        free(map.cells[i]);
    }
    free(map.cells);
    printf("Map destroyed\n");
}