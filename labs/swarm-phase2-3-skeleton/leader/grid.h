/* grid.h — the occupancy grid of one leader (given interface; YOU implement grid.c, Task 3).
 * Each cell holds the id of the follower that is in it, or is empty. */
#ifndef GRID_H
#define GRID_H

typedef struct grid grid_t;

/* per_row_locks == 0: ONE mutex for the whole grid.  != 0: one mutex per row. */
grid_t *grid_create(int w, int h, int per_row_locks);
void    grid_destroy(grid_t *g);

/* Put follower `id` (>= 0) into (x, y): 0 ok, -1 if the cell is occupied or outside the grid. */
int     grid_claim(grid_t *g, int x, int y, int id);

/* Remove follower `id` from (x, y). It is an error (returns -1) if the cell does not hold `id`. */
int     grid_release(grid_t *g, int x, int y, int id);

/* Id of the follower in (x, y), or -1 if empty or outside. */
int     grid_owner(grid_t *g, int x, int y);

/* Exchange the contents of two cells (both may be occupied). Used by tools/swapstress.c.
 * 0 ok, -1 if a cell is outside the grid. KNOWN BUG in per-row mode: see Task 4. */
int     grid_swap(grid_t *g, int x1, int y1, int x2, int y2);

#endif
