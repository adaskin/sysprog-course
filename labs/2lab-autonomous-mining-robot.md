# Lab: Autonomous Mining Robot Exploration

**Course:** BIL 322 System Programming  
**Topic:** C arrays, structs, file I/O, function pointers, Makefiles, modular design, optional GUI  
**Mode:** Individual or pair work  
**Deadline:** Extended — you have several days to complete this (not a 2-hour in-class exercise)  
**AI use:** Allowed, but must be disclosed and you must be able to explain all code

---

## 1. Story

A mining company uses small autonomous robots to explore dangerous underground areas.

The mine is represented as a 2D grid. Some cells are safe, some contain hazards (unstable ground, gas pockets, etc.), and some contain valuable ore. The robot starts from a base cell and must move through the mine to collect ore while avoiding hazards.

The robot has a simple decision policy. Different policies produce different behavior:

| Policy | Behavior |
|---|---|
| Greedy | Move toward the nearest ore cell as fast as possible |
| Cautious | Avoid high-risk areas, prefer explored cells |
| Explorer | Prefer unexplored territory over known areas |
| Random | Move randomly among safe neighboring cells |

Your task is to implement the mine model, robot movement, decision policies, CLI simulation, and optionally a graphical view.

### Why this lab matters

This lab practices several C and system-programming fundamentals:

- **2D arrays** for the grid
- **Structs** for map state and robot state
- **File I/O** for loading maps
- **Function pointers** for selectable decision policies
- **Makefiles** for multi-file builds
- **Modular design** (model / robot / policy / controller / view)

It also connects to later course topics:

| Later topic | Connection to this lab |
|---|---|
| Processes | Each robot could run as a separate process |
| Threads | Multiple robots sharing one map concurrently |
| IPC | Robots send sensor readings to a controller |
| Signals | Emergency stop via `SIGINT` or `SIGUSR1` |
| Sockets | Remote controller and robot clients |

---

## 2. How the Pieces Fit Together

Before diving into code, here is a high-level view of the architecture:

```text
┌──────────────────────────────────────────────────────┐
│                     main.c                           │
│  Parses CLI arguments, loads map, starts simulation  │
└───────────────────────┬──────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│   model.c    │ │   robot.c    │ │  policy.c    │
│              │ │              │ │              │
│ Map grid     │ │ Robot state  │ │ Decision     │
│ Hazards/ore  │ │ Movement     │ │ policies     │
│ Sensor data  │ │ Energy       │ │ (fn ptrs)    │
└──────┬───────┘ └──────┬───────┘ └──────┬───────┘
       │                │                │
       └────────────────┼────────────────┘
                        ▼
              ┌──────────────────┐
              │  controller.c    │
              │  Simulation loop │
              └────────┬─────────┘
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
  ┌──────────────┐         ┌──────────────┐
  │ view_cli.c   │         │ view_gui.c   │
  │ Terminal     │         │ SDL2/GTK     │
  │ rendering    │         │ (optional)   │
  └──────────────┘         └──────────────┘
```

### The function pointer pattern

One of the key C concepts in this lab is the **function pointer table**.

Instead of writing:

```c
if (strcmp(policy_name, "greedy") == 0) {
    move = do_greedy(map, robot);
} else if (strcmp(policy_name, "cautious") == 0) {
    move = do_cautious(map, robot);
}
// ... more else-ifs
```

We store policies in a table:

```c
typedef Move (*policy_fn)(const Map *m, const Robot *r);

typedef struct {
    const char *name;
    policy_fn choose;
} Policy;

static const Policy policies[] = {
    { "greedy",   policy_greedy_ore },
    { "cautious", policy_cautious },
    { "explorer", policy_explorer },
    { "random",   policy_random },
};
```

Then the controller simply calls:

```c
Move mv = p->choose(m, r);
```

This pattern is used extensively in real systems: callback tables, plugin architectures, virtual method tables in C++, and event handlers in GUIs.

### What the robot "knows" (sensor model)

The robot has limited sensing:

- It knows its own position.
- It can check whether a neighboring cell is a hazard (sensor detects danger ahead).
- It knows the `hazard_count` for each explored cell (how many hazards surround that cell).
- It does **not** see the full map from the start (cells are unexplored until visited or revealed).

For this lab, the policy functions **can** access the full map data structure. The "fog of war" (hiding unexplored ore from the robot) is an **optional bonus** feature. The basic requirement is that the robot will not move into a hazard cell.

---

## 3. Map Format

The map file contains:

```text
ROWS COLS
grid characters...
```

Characters:

| Character | Meaning |
|---|---|
| `.` | Safe cell |
| `H` | Hazard cell |
| `O` | Ore cell |
| `B` | Base / robot start position |

Whitespace between characters is optional but recommended for readability.

Example `maps/mine1.txt`:

```text
10 14
. . . . H . . . . . . O . .
. H . . . . O . H . . . . .
. . . O . . . . . . H . . .
. . H . . H . . O . . . O .
. . . . . . . . . . H . . .
B . . O . . H . . . . . . .
. H . . . . . . O . H . . .
. . . H . O . . . . . . H .
. O . . . . H . . O . . . .
. . . . H . . . . . . O . .
```

If there is no `B` character, the robot starts at cell `(0, 0)`.

---

## 4. Required Features

You must implement the following. The skeleton code provides the structure; you fill in the `TODO` sections.

### 4.1 `model.c` — Map loading and sensor initialization

| Function | What it does |
|---|---|
| `map_load()` | Read a map file into the `Map` struct |
| `map_generate_random()` | Generate a random map (used when no `--map` is given) |
| `map_init_sensor()` | Compute `hazard_count[r][c]` for every cell |

The sensor initialization is the same logic as Minesweeper neighbor counting:

> For each safe cell, count how many of its 8 neighbors are hazards.  
> For hazard cells, set `hazard_count = -1`.

### 4.2 `robot.c` — Robot state and movement

Already mostly provided. You should understand how `robot_apply_move` works:

- Checks bounds.
- Refuses to move into hazards.
- Decrements energy.
- Marks robot inactive if it hits a hazard or goes out of bounds.

### 4.3 `policy.c` — Decision policies (the main coding task)

You must implement **at least two** of these three policies:

| Policy | Goal |
|---|---|
| `policy_greedy_ore` | Move toward nearest ore |
| `policy_cautious` | Avoid high hazard_count areas |
| `policy_explorer` | Prefer unexplored cells |

`policy_random` is already implemented.

**How to implement a policy:**

Each policy function receives the full map and the robot's current position. It returns a `Move` enum value (`MOVE_UP`, `MOVE_DOWN`, `MOVE_LEFT`, `MOVE_RIGHT`, or `MOVE_NONE`).

A common approach:

1. Loop over the 4 possible directions.
2. For each direction, compute the neighbor cell `(nr, nc)`.
3. Skip if out of bounds or hazard.
4. Score the neighbor cell (lower score = better).
5. Return the move with the best (lowest) score.

Example scoring for the **cautious** policy:

```text
score = hazard_count[nr][nc] * 10
if (explored[nr][nc]) score -= 5    // prefer explored
```

Example scoring for the **explorer** policy:

```text
score = 0   if NOT explored
score = 100 if already explored
score += hazard_count[nr][nc]       // tie-breaker
```

### 4.4 `controller.c` — Simulation loop

Mostly provided. Understand the loop:

```text
while (robot active AND steps < max_steps AND ore remains):
    ask policy for a move
    apply the move
    mark cell explored
    collect ore if present
    decrement energy
```

### 4.5 `view_cli.c` — Terminal rendering

Provided. The view uses these symbols:

| Symbol | Meaning |
|---|---|
| `R` | Robot current position |
| `B` | Base cell |
| `H` | Hazard |
| `O` | Ore (remaining) |
| `.` | Explored safe cell |
| `?` | Unexplored cell |

### 4.6 `Makefile`

The skeleton includes a Makefile. You should understand:

- `make` builds the CLI version.
- `make gui` builds the SDL2 GUI version (optional).
- `make clean` removes binaries.

### 4.7 `report.md`

See Section 12 for requirements.

---

## 5. Optional / Bonus Features

Bonus points may be given for:

| Feature | Description |
|---|---|
| GUI view | SDL2, GTK, or Qt rendering of the map and robot |
| Extra policies | e.g., "return to base when energy low", "A* pathfinding to ore" |
| Fog of war | Robot cannot see ore until adjacent; policy only uses explored data |
| Energy management | Robot must return to base to recharge |
| Flood-fill exploration | Auto-reveal safe connected region when robot enters a zero-hazard cell |
| Multiple robots | Two or more robots exploring cooperatively |
| Random map validation | Ensure map is solvable (ore reachable from base) |
| Benchmarking | Compare policies: which collects most ore in fewest steps? |

---

## 6. GUI Options (Optional)

The skeleton provides an SDL2-based GUI. If you prefer a different toolkit, you may use it instead.

### Option A: SDL2 (provided skeleton)

SDL2 is a lightweight graphics library. Good for simple 2D rendering.

Install:

```bash
sudo apt install libsdl2-dev
```

The skeleton `view_gui.c` provides the window/renderer setup. You implement `draw_map()` to draw colored rectangles for each cell.

### Option B: GTK

GTK is a full widget toolkit (buttons, windows, drawing areas). You used it in the earlier Minesweeper lab.

Install:

```bash
sudo apt install libgtk-4-dev
```

Compile with:

```bash
gcc main.c model.c robot.c policy.c controller.c view_gui_gtk.c \
    $(pkg-config --cflags --libs gtk4) -DWITH_GUI -o minerobot_gui
```

You would create a `GtkDrawingArea` and draw cells in the draw callback, similar to the Minesweeper view.

### Option C: Qt

Qt is a cross-platform framework. Requires C++ for most usage, but has C bindings.

If you choose Qt, you will need to wrap the C model in a thin C++ layer. This is more work but gives you a polished UI.

### Which should I choose?

| If you want... | Use |
|---|---|
| Simplest setup, just colored rectangles | SDL2 |
| Widget-based UI similar to Minesweeper lab | GTK |
| Modern cross-platform desktop app | Qt (more effort) |
| No GUI at all (focus on C logic) | Skip GUI, do CLI only |

The GUI is entirely optional. Full marks are possible without it.

---

## 7. Getting Started (Suggested Work Order)

Since you have several days, here is a suggested approach:

1. **Day 1: Understand the skeleton.**
   - Clone the template repo.
   - Read through `model.h`, `robot.h`, `policy.h`.
   - Try `make` — it should compile (with warnings about unimplemented functions).
   - Run `./minerobot --help` to see options.

2. **Day 1–2: Implement `map_load` and `map_init_sensor`.**
   - Test with `maps/mine1.txt`.
   - Add a temporary `printf` in `map_init_sensor` to verify hazard counts.

3. **Day 2: Implement `map_generate_random`.**
   - Test with `--rows 10 --cols 14 --hazard 12 --ore 8 --seed 42`.

4. **Day 2–3: Implement two policies.**
   - Start with `policy_explorer` (simplest scoring).
   - Then implement `policy_greedy_ore` or `policy_cautious`.
   - Test each policy and observe different behavior.

5. **Day 3: Test, debug, write report.**
   - Run all policies on the same map.
   - Compare outputs.
   - Fix edge cases (robot stuck at boundary, no ore remaining, etc.).
   - Write `report.md`.

6. **Optional: GUI or bonus features.**

---

## 8. Detailed Task Descriptions

### Task 1: Implement `map_load`

```c
int map_load(Map *m, const char *filename);
```

Steps:

1. Open the file with `fopen(filename, "r")`.
2. Read `rows` and `cols` with `fscanf`.
3. Validate: `rows <= MAP_MAX`, `cols <= MAP_MAX`.
4. Clear all arrays with `memset`.
5. Read `rows * cols` non-whitespace characters. Use `fscanf(file, " %c", &ch)` — the space before `%c` skips whitespace/newlines.
6. For each character:
   - `'.'` → safe (do nothing)
   - `'H'` → set `hazard[r][c] = 1`
   - `'O'` → set `ore[r][c] = 1`
   - `'B'` → set `base_row = r`, `base_col = c`
7. If no `'B'` found, use `(0, 0)` as base.
8. Return `0` on success, `-1` on error.

### Task 2: Implement `map_init_sensor`

```c
void map_init_sensor(Map *m);
```

For each cell `(r, c)`:

- If `hazard[r][c] == 1`, set `hazard_count[r][c] = -1`.
- Otherwise, count hazards in the 8 surrounding cells:

```c
for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
        if (dr == 0 && dc == 0) continue; // skip self
        int nr = r + dr;
        int nc = c + dc;
        if (map_in_bounds(m, nr, nc) && m->hazard[nr][nc]) {
            count++;
        }
    }
}
hazard_count[r][c] = count;
```

### Task 3: Implement `map_generate_random`

```c
int map_generate_random(Map *m, int rows, int cols,
                        int hazard_percent, int ore_percent,
                        unsigned int seed);
```

Steps:

1. Validate dimensions.
2. Clear arrays.
3. For each cell, generate a random number 0–99.
   - If `< hazard_percent`, place hazard.
   - Else if `< hazard_percent + ore_percent`, place ore.
4. Ensure `(0, 0)` or base cell is safe and not ore.
5. Return 0.

### Task 4: Implement decision policies

See Section 4.3 above for scoring suggestions.

Key implementation pattern:

```c
Move policy_explorer(const Map *m, const Robot *r) {
    int best_score = INT_MAX;
    Move best_move = MOVE_NONE;

    for (int i = 0; i < 4; i++) {
        int nr = r->row + deltas[i].dr;
        int nc = r->col + deltas[i].dc;

        // Skip invalid moves
        if (!map_in_bounds(m, nr, nc)) continue;
        if (m->hazard[nr][nc]) continue;

        // Score this neighbor
        int score = 0;
        if (m->explored[nr][nc]) {
            score += 100;  // prefer unexplored
        }
        score += m->hazard_count[nr][nc];  // tie-breaker

        if (score < best_score) {
            best_score = score;
            best_move = deltas[i].mv;
        }
    }

    return best_move;
}
```

### Task 5: Test all policies

```bash
./minerobot --map maps/mine1.txt --policy greedy
./minerobot --map maps/mine1.txt --policy cautious
./minerobot --map maps/mine1.txt --policy explorer
./minerobot --map maps/mine1.txt --policy random
```

Observe and note differences in your report.

### Task 6 (Optional): GUI

See Section 6 for toolkit options.

For SDL2, implement `draw_map()`:

```c
static void draw_map(SDL_Renderer *renderer, const Map *m, const Robot *r) {
    for (int row = 0; row < m->rows; row++) {
        for (int col = 0; col < m->cols; col++) {
            SDL_Rect rect = {
                col * CELL_SIZE,
                row * CELL_SIZE,
                CELL_SIZE,
                CELL_SIZE
            };

            // Choose color based on cell state
            if (row == r->row && col == r->col) {
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // green robot
            } else if (m->hazard[row][col]) {
                SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // red hazard
            } else if (m->ore[row][col]) {
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); // yellow ore
            } else if (m->explored[row][col]) {
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); // light gray
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // dark gray
            }

            SDL_RenderFillRect(renderer, &rect);
        }
    }
}
```

---

## 9. Expected CLI Output

```text
Map size: 10x14
Policy: cautious
Initial ore: 12

Step 0
  01234567890123
0 ????H??????O??
1 ?H????O?H?????
2 ???O??????H???
3 ??H??H??O???O?
4 ??????????H???
5 B??O??H???????
6 ?H??????O?H???
7 ???H?O?????H??
8 ?O????H??O????
9 ????H??????O??

...

Final state:
  01234567890123
0 ....H....1.O..
1 .H.1..O.H....
2 ..1O..1..1H...
3 ..H11H..O..1O.
4 ....1.....H...
5 B..O..H.......
6 .H.1....O.H...
7 ..1H.O....1H..
8 .O..1.H..O....
9 ....H......O..

Robot active: no
Steps: 47
Ore collected: 8
Remaining ore: 4
Energy: 153
```

Your exact output will differ depending on policy and map.

---

## 10. Grading

### Required (100 points total)

| Item | Points |
|---|---:|
| Project builds with `make` (no errors) | 10 |
| Git repository organization and commit history | 10 |
| `map_load` works correctly | 15 |
| `map_generate_random` works correctly | 10 |
| `map_init_sensor` computes correct counts | 15 |
| Robot movement, energy, and state management | 10 |
| At least two decision policies implemented and working | 25 |
| CLI simulation runs and prints summary | 10 |
| `report.md` with AI disclosure | 5 |
| **Total** | **100** |

### Bonus (capped at +20 total bonus)

| Bonus item | Points |
|---|---:|
| SDL2 or GTK/Qt GUI | up to 15 |
| Third decision policy | up to 5 |
| Fog of war (hide unexplored ore from policy) | up to 5 |
| Energy management / return-to-base | up to 5 |
| Flood-fill exploration | up to 5 |
| Multiple robots | up to 10 |
| Benchmarking / comparison table in report | up to 5 |

---

## 11. Report Requirements

Create `report.md` in your repository with:

1. **Your name and student ID.**
2. **Brief design explanation** — how the modules interact.
3. **Policy descriptions** — explain each policy you implemented and its scoring logic.
4. **Example commands and outputs** — show at least two different policies running on the same map.
5. **Known bugs or limitations** — be honest.
6. **AI usage disclosure** — which tools, what for.

Example AI disclosure:

```text
I used Qwen to help debug a segfault in map_load (I was reading
past the end of the file). I also asked it to explain the
function pointer syntax for the policy table. All final code
was written, tested, and understood by me.
```

---

## 12. AI Use Policy

AI use is **allowed** for this lab.

You may use AI for:

- Explaining C compiler errors or warnings
- Suggesting algorithms or scoring approaches
- Debugging linker errors or Makefile issues
- Generating test maps
- Explaining SDL2/GTK/Qt APIs
- Improving code style or readability
- Explaining function pointer syntax

However:

- You must **understand** the final code.
- You must **test** it yourself.
- You must **disclose** which AI tools you used and for what.
- You must **not** submit code you cannot explain if asked.

The instructor may ask you to explain any part of your submission orally.

---

## 13. Submission

Push to your GitHub Org repository, share link through classroom.google.com.

Required files:

```text
Makefile
main.c
model.h / model.c
robot.h / robot.c
policy.h / policy.c
controller.h / controller.c
view_cli.h / view_cli.c
view_gui.h / view_gui.c   (even if GUI not implemented, keep the stub)
maps/mine1.txt
report.md
```

Optional additions:

```text
maps/mine2.txt
maps/random_tests/
screenshots/
```

Final check before submission:

```bash
make clean
make
./minerobot --map maps/mine1.txt --policy greedy
./minerobot --map maps/mine1.txt --policy cautious
```

If you implemented GUI:

```bash
make clean
make gui
./minerobot_gui --map maps/mine1.txt --policy cautious --gui --delay 100
```

---

## 14. Possible Extensions for the Final Project

This lab can later become part of a larger system-programming project.

| Course topic | Extension |
|---|---|
| Processes | Each robot runs as a separate process |
| Pipes | Robots send sensor readings to controller via pipes |
| Threads | Multiple robots share one map concurrently |
| Synchronization | Protect shared map with mutexes |
| Deadlock | Robots block each other in narrow tunnels |
| Signals | Emergency stop with `SIGINT` or `SIGUSR1` |
| Sockets | Remote controller and robot clients over TCP |
| GUI | Real-time visualization of multi-agent coordination |

This makes the lab useful not only as a C practice exercise, but also as the foundation for a larger AI/robotics/systems project later in the semester.

---

---

# Skeleton Code

> **Note:** This skeleton code is also available in your GitHub Org repository. You do not need to copy it from this document — clone the repo and start editing.

The skeleton provides:

- All header files (`.h`) with complete type definitions and function declarations.
- Most of `robot.c`, `controller.c`, `view_cli.c`, and `main.c` already implemented.
- `policy.c` with `policy_random` implemented and the other three as `TODO`.
- `model.c` with helper functions implemented and `map_load`, `map_generate_random`, `map_init_sensor` as `TODO`.
- `view_gui.c` with SDL2 window setup and `draw_map` as `TODO`.
- A complete `Makefile`.

Your main work is in: **`model.c`** (3 functions) and **`policy.c`** (2–3 functions).

---

## `model.h`

```c
#ifndef MODEL_H
#define MODEL_H

#define MAP_MAX 32

typedef struct {
    int rows;
    int cols;

    int hazard[MAP_MAX][MAP_MAX];       /* 1 if hazard */
    int ore[MAP_MAX][MAP_MAX];          /* 1 if ore present */
    int explored[MAP_MAX][MAP_MAX];     /* 1 if robot has visited/exposed */
    int hazard_count[MAP_MAX][MAP_MAX]; /* nearby hazard count */

    int base_row;
    int base_col;
} Map;

int map_load(Map *m, const char *filename);
int map_generate_random(Map *m, int rows, int cols,
                        int hazard_percent, int ore_percent,
                        unsigned int seed);

void map_init_sensor(Map *m);

int map_in_bounds(const Map *m, int r, int c);
int map_is_safe(const Map *m, int r, int c);
int map_has_ore(const Map *m, int r, int c);
int map_count_ore(const Map *m);

void map_mark_explored(Map *m, int r, int c);
void map_reveal_safe_area(Map *m, int r, int c); /* optional bonus */

#endif
```

---

## `model.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

int map_in_bounds(const Map *m, int r, int c) {
    return r >= 0 && r < m->rows && c >= 0 && c < m->cols;
}

int map_is_safe(const Map *m, int r, int c) {
    return map_in_bounds(m, r, c) && !m->hazard[r][c];
}

int map_has_ore(const Map *m, int r, int c) {
    return map_in_bounds(m, r, c) && m->ore[r][c];
}

int map_count_ore(const Map *m) {
    int count = 0;

    for (int r = 0; r < m->rows; r++) {
        for (int c = 0; c < m->cols; c++) {
            if (m->ore[r][c]) {
                count++;
            }
        }
    }

    return count;
}

void map_mark_explored(Map *m, int r, int c) {
    if (map_in_bounds(m, r, c)) {
        m->explored[r][c] = 1;
    }
}

void map_reveal_safe_area(Map *m, int r, int c) {
    /* Optional bonus:
       Implement flood-fill reveal for safe connected cells.
       Similar to Minesweeper: if hazard_count[r][c] == 0,
       recursively reveal all safe neighbors.
    */
}

int map_load(Map *m, const char *filename) {
    /* TODO:
       1. Open file.
       2. Read rows and cols.
       3. Validate dimensions (must be <= MAP_MAX).
       4. Clear map arrays with memset.
       5. Read ROWS * COLS non-whitespace characters.
          Hint: fscanf(file, " %c", &ch) skips whitespace.
       6. Fill hazard and ore arrays based on character.
       7. If character is 'B', set base_row/base_col.
       8. If no 'B' is found, use (0, 0) as base.
       9. Return 0 on success, -1 on failure.

       Characters:
       '.' safe
       'H' hazard
       'O' ore
       'B' base/start
    */

    return -1;
}

int map_generate_random(Map *m, int rows, int cols,
                        int hazard_percent, int ore_percent,
                        unsigned int seed) {
    /* TODO:
       1. Validate rows/cols (must be > 0 and <= MAP_MAX).
       2. Clear map arrays with memset.
       3. Set m->rows and m->cols.
       4. For each cell, generate a random number 0-99.
          - If < hazard_percent: place hazard.
          - Else if < hazard_percent + ore_percent: place ore.
          - Otherwise: safe.
       5. Ensure base cell (0,0) is safe and not ore.
       6. Set base_row = 0, base_col = 0.
       7. Return 0 on success.
    */

    return -1;
}

void map_init_sensor(Map *m) {
    /* TODO:
       For every cell (r, c):
       - If hazard[r][c] == 1, set hazard_count[r][c] = -1.
       - Otherwise, count hazards in the 8 surrounding cells
         (do not count the cell itself).
         Store the count in hazard_count[r][c].
    */
}
```

---

## `robot.h`

```c
#ifndef ROBOT_H
#define ROBOT_H

#include "model.h"

typedef enum {
    MOVE_NONE,
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT
} Move;

typedef struct {
    int row;
    int col;

    int steps;
    int ore_collected;
    int energy;

    int active;
} Robot;

void robot_init(Robot *r, int row, int col, int energy);
int robot_apply_move(Robot *r, const Map *m, Move mv);
const char *move_name(Move mv);

#endif
```

---

## `robot.c`

```c
#include <stdio.h>

#include "robot.h"

void robot_init(Robot *r, int row, int col, int energy) {
    r->row = row;
    r->col = col;
    r->steps = 0;
    r->ore_collected = 0;
    r->energy = energy;
    r->active = 1;
}

const char *move_name(Move mv) {
    switch (mv) {
        case MOVE_UP:    return "UP";
        case MOVE_DOWN:  return "DOWN";
        case MOVE_LEFT:  return "LEFT";
        case MOVE_RIGHT: return "RIGHT";
        default:         return "NONE";
    }
}

int robot_apply_move(Robot *r, const Map *m, Move mv) {
    int nr = r->row;
    int nc = r->col;

    switch (mv) {
        case MOVE_UP:
            nr--;
            break;
        case MOVE_DOWN:
            nr++;
            break;
        case MOVE_LEFT:
            nc--;
            break;
        case MOVE_RIGHT:
            nc++;
            break;
        case MOVE_NONE:
        default:
            r->active = 0;
            return -1;
    }

    if (!map_in_bounds(m, nr, nc)) {
        r->active = 0;
        return -1;
    }

    if (m->hazard[nr][nc]) {
        r->active = 0;
        return -1;
    }

    r->row = nr;
    r->col = nc;
    r->steps++;

    return 0;
}
```

---

## `policy.h`

```c
#ifndef POLICY_H
#define POLICY_H

#include "model.h"
#include "robot.h"

/* A policy is a function that takes the map and robot state,
   and returns the next move. */
typedef Move (*policy_fn)(const Map *m, const Robot *r);

typedef struct {
    const char *name;
    policy_fn choose;
} Policy;

Move policy_greedy_ore(const Map *m, const Robot *r);
Move policy_cautious(const Map *m, const Robot *r);
Move policy_explorer(const Map *m, const Robot *r);
Move policy_random(const Map *m, const Robot *r);

const Policy *policy_find(const char *name);
void policy_list(void);

#endif
```

---

## `policy.c`

```c
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "policy.h"

typedef struct {
    Move mv;
    int dr;
    int dc;
} MoveDelta;

static const MoveDelta deltas[] = {
    { MOVE_UP,    -1,  0 },
    { MOVE_DOWN,   1,  0 },
    { MOVE_LEFT,   0, -1 },
    { MOVE_RIGHT,  0,  1 },
};

#define NUM_DIRECTIONS 4

static int manhattan(int r1, int c1, int r2, int c2) {
    int dr = r1 - r2;
    int dc = c1 - c2;

    if (dr < 0) dr = -dr;
    if (dc < 0) dc = -dc;

    return dr + dc;
}

static int valid_neighbor(const Map *m, const Robot *r,
                          const MoveDelta *d,
                          int *nr, int *nc) {
    int tr = r->row + d->dr;
    int tc = r->col + d->dc;

    if (!map_in_bounds(m, tr, tc)) {
        return 0;
    }

    if (m->hazard[tr][tc]) {
        return 0;
    }

    if (nr) *nr = tr;
    if (nc) *nc = tc;

    return 1;
}

static int find_nearest_ore(const Map *m, int r, int c,
                            int *best_r, int *best_c) {
    /* TODO:
       Scan the whole map and find the ore cell with the
       smallest Manhattan distance to (r, c).

       Set *best_r and *best_c to that cell.
       Return 1 if ore exists, 0 otherwise.
    */

    int best_dist = INT_MAX;
    int found = 0;

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            if (m->ore[i][j]) {
                int dist = manhattan(r, c, i, j);
                if (dist < best_dist) {
                    best_dist = dist;
                    *best_r = i;
                    *best_c = j;
                    found = 1;
                }
            }
        }
    }

    return found;
}

Move policy_greedy_ore(const Map *m, const Robot *r) {
    /* TODO:
       Choose the safe neighboring cell that moves the robot
       closest to the nearest ore.

       Suggested method:
       1. Find nearest ore from current position.
       2. For each valid neighbor:
          - compute Manhattan distance from neighbor to that ore.
       3. Choose neighbor with smallest distance.
       4. If no ore remains, return MOVE_NONE.
    */

    return MOVE_NONE;
}

Move policy_cautious(const Map *m, const Robot *r) {
    /* TODO:
       Choose a safe neighbor with low hazard_count.

       Suggested scoring:
       score = hazard_count[nr][nc] * 10
       if (explored[nr][nc]) score -= 5;  // slight preference

       Choose the neighbor with the lowest score.
    */

    return MOVE_NONE;
}

Move policy_explorer(const Map *m, const Robot *r) {
    /* TODO:
       Prefer unexplored safe cells.

       Suggested scoring:
       score = 0   if NOT explored
       score = 100 if already explored
       score += hazard_count[nr][nc]  // tie-breaker

       Choose the neighbor with the lowest score.
    */

    return MOVE_NONE;
}

Move policy_random(const Map *m, const Robot *r) {
    Move options[NUM_DIRECTIONS];
    int n = 0;

    for (int i = 0; i < NUM_DIRECTIONS; i++) {
        int nr, nc;

        if (valid_neighbor(m, r, &deltas[i], &nr, &nc)) {
            options[n++] = deltas[i].mv;
        }
    }

    if (n == 0) {
        return MOVE_NONE;
    }

    return options[rand() % n];
}

static const Policy policies[] = {
    { "greedy",   policy_greedy_ore },
    { "cautious", policy_cautious },
    { "explorer", policy_explorer },
    { "random",   policy_random },
};

const Policy *policy_find(const char *name) {
    int count = sizeof(policies) / sizeof(policies[0]);

    for (int i = 0; i < count; i++) {
        if (strcmp(policies[i].name, name) == 0) {
            return &policies[i];
        }
    }

    return NULL;
}

void policy_list(void) {
    int count = sizeof(policies) / sizeof(policies[0]);

    fprintf(stderr, "Available policies:\n");

    for (int i = 0; i < count; i++) {
        fprintf(stderr, "  %s\n", policies[i].name);
    }
}
```

---

## `controller.h`

```c
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "model.h"
#include "robot.h"
#include "policy.h"

typedef struct {
    int max_steps;
    int delay_ms;
    int verbose;
} SimConfig;

void controller_step(Map *m, Robot *r, const Policy *p);
int controller_run_cli(Map *m, Robot *r, const Policy *p,
                       const SimConfig *cfg);

#endif
```

---

## `controller.c`

```c
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <time.h>

#include "controller.h"
#include "view_cli.h"

static void sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }

    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    nanosleep(&ts, NULL);
}

void controller_step(Map *m, Robot *r, const Policy *p) {
    if (!r->active) {
        return;
    }

    Move mv = p->choose(m, r);

    if (mv == MOVE_NONE) {
        r->active = 0;
        return;
    }

    if (robot_apply_move(r, m, mv) != 0) {
        r->active = 0;
        return;
    }

    map_mark_explored(m, r->row, r->col);

    if (m->ore[r->row][r->col]) {
        m->ore[r->row][r->col] = 0;
        r->ore_collected++;
    }

    r->energy--;

    if (r->energy <= 0) {
        r->active = 0;
    }
}

int controller_run_cli(Map *m, Robot *r, const Policy *p,
                       const SimConfig *cfg) {
    int step = 0;

    while (r->active &&
           step < cfg->max_steps &&
           map_count_ore(m) > 0) {

        if (cfg->verbose) {
            printf("\nStep %d\n", step);
            view_cli_print(m, r);
        }

        controller_step(m, r, p);

        if (cfg->delay_ms > 0) {
            sleep_ms(cfg->delay_ms);
        }

        step++;
    }

    printf("\nFinal state:\n");
    view_cli_print(m, r);
    view_cli_print_summary(m, r);

    return 0;
}
```

---

## `view_cli.h`

```c
#ifndef VIEW_CLI_H
#define VIEW_CLI_H

#include "model.h"
#include "robot.h"

void view_cli_print(const Map *m, const Robot *r);
void view_cli_print_summary(const Map *m, const Robot *r);

#endif
```

---

## `view_cli.c`

```c
#include <stdio.h>

#include "view_cli.h"

void view_cli_print(const Map *m, const Robot *r) {
    printf("  ");

    for (int c = 0; c < m->cols; c++) {
        printf("%d", c % 10);
    }

    printf("\n");

    for (int row = 0; row < m->rows; row++) {
        printf("%d ", row % 10);

        for (int col = 0; col < m->cols; col++) {
            char ch = '?';

            if (row == r->row && col == r->col) {
                ch = 'R';
            } else if (m->hazard[row][col]) {
                ch = 'H';
            } else if (m->ore[row][col]) {
                ch = 'O';
            } else if (row == m->base_row && col == m->base_col) {
                ch = 'B';
            } else if (m->explored[row][col]) {
                ch = '.';
            } else {
                ch = '?';
            }

            printf("%c", ch);
        }

        printf("\n");
    }
}

void view_cli_print_summary(const Map *m, const Robot *r) {
    printf("Robot active: %s\n", r->active ? "yes" : "no");
    printf("Steps: %d\n", r->steps);
    printf("Ore collected: %d\n", r->ore_collected);
    printf("Remaining ore: %d\n", map_count_ore(m));
    printf("Energy: %d\n", r->energy);
}
```

---

## `view_gui.h`

```c
#ifndef VIEW_GUI_H
#define VIEW_GUI_H

#include "model.h"
#include "robot.h"
#include "policy.h"

#ifdef WITH_GUI

int view_gui_run(Map *m, Robot *r, const Policy *p,
                 int max_steps, int delay_ms);

#endif

#endif
```

---

## `view_gui.c`

```c
#ifdef WITH_GUI

#include <SDL2/SDL.h>
#include <stdio.h>

#include "view_gui.h"
#include "controller.h"

#define CELL_SIZE 32

static void draw_map(SDL_Renderer *renderer, const Map *m, const Robot *r) {
    /* TODO:
       Draw each cell as a colored rectangle.

       Suggested colors:
       - robot:          green  (0, 200, 0)
       - hazard:         red    (200, 0, 0)
       - ore:            yellow (200, 200, 0)
       - base:           blue   (0, 0, 200)
       - explored safe:  light gray (180, 180, 180)
       - unexplored:     dark gray  (100, 100, 100)

       Use SDL_SetRenderDrawColor + SDL_RenderFillRect.
       Each cell is CELL_SIZE x CELL_SIZE pixels.
       Position: x = col * CELL_SIZE, y = row * CELL_SIZE.
    */
}

int view_gui_run(Map *m, Robot *r, const Policy *p,
                 int max_steps, int delay_ms) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int width = m->cols * CELL_SIZE;
    int height = m->rows * CELL_SIZE;

    SDL_Window *window =
        SDL_CreateWindow("Mining Robot",
                         SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED,
                         width, height,
                         0);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    int step = 0;

    while (running &&
           r->active &&
           step < max_steps &&
           map_count_ore(m) > 0) {

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE) {
                running = 0;
            }
        }

        controller_step(m, r, p);

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        draw_map(renderer, m, r);

        SDL_RenderPresent(renderer);

        if (delay_ms > 0) {
            SDL_Delay(delay_ms);
        }

        step++;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#endif
```

---

## `main.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "model.h"
#include "robot.h"
#include "policy.h"
#include "controller.h"
#include "view_cli.h"

#ifdef WITH_GUI
#include "view_gui.h"
#endif

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "Options:\n"
            "  --map FILE        Load map file\n"
            "  --policy NAME     Decision policy: greedy, cautious, explorer, random\n"
            "  --steps N         Maximum simulation steps (default: 200)\n"
            "  --delay MS        Delay between steps in ms (default: 0)\n"
            "  --seed N          Random seed\n"
            "  --rows N          Rows for random map (default: 10)\n"
            "  --cols N          Columns for random map (default: 14)\n"
            "  --hazard P        Hazard percentage for random map (default: 12)\n"
            "  --ore P           Ore percentage for random map (default: 8)\n"
            "  --gui             Use GUI if compiled with GUI support\n"
            "  --help            Show this help\n",
            prog);

    policy_list();
}

int main(int argc, char **argv) {
    Map m;
    Robot r;

    SimConfig cfg;
    cfg.max_steps = 200;
    cfg.delay_ms = 0;
    cfg.verbose = 1;

    char map_file[512] = "";
    char policy_name[64] = "cautious";

    int use_gui = 0;
    int rows = 10;
    int cols = 14;
    int hazard_percent = 12;
    int ore_percent = 8;
    unsigned int seed = (unsigned int)time(NULL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            snprintf(map_file, sizeof(map_file), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            snprintf(policy_name, sizeof(policy_name), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            cfg.max_steps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            cfg.delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (unsigned int)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rows") == 0 && i + 1 < argc) {
            rows = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--cols") == 0 && i + 1 < argc) {
            cols = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--hazard") == 0 && i + 1 < argc) {
            hazard_percent = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ore") == 0 && i + 1 < argc) {
            ore_percent = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--gui") == 0) {
            use_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    srand(seed);

    memset(&m, 0, sizeof(m));

    if (map_file[0] != '\0') {
        if (map_load(&m, map_file) != 0) {
            fprintf(stderr, "Failed to load map: %s\n", map_file);
            return 1;
        }
    } else {
        if (map_generate_random(&m, rows, cols,
                                hazard_percent, ore_percent,
                                seed) != 0) {
            fprintf(stderr, "Failed to generate random map\n");
            return 1;
        }
    }

    map_init_sensor(&m);

    robot_init(&r, m.base_row, m.base_col, 200);
    map_mark_explored(&m, r.row, r.col);

    const Policy *p = policy_find(policy_name);

    if (!p) {
        fprintf(stderr, "Unknown policy: %s\n", policy_name);
        policy_list();
        return 1;
    }

    printf("Map size: %dx%d\n", m.rows, m.cols);
    printf("Policy: %s\n", p->name);
    printf("Initial ore: %d\n", map_count_ore(&m));

    if (use_gui) {
#ifdef WITH_GUI
        return view_gui_run(&m, &r, p, cfg.max_steps, cfg.delay_ms);
#else
        fprintf(stderr, "GUI support is not enabled. Build with: make gui\n");
        return 1;
#endif
    }

    return controller_run_cli(&m, &r, p, &cfg);
}
```

---

## `Makefile`

> **Important:** Makefile recipe lines must begin with real TAB characters, not spaces.

```make
CC = gcc
CFLAGS ?= -Wall -Wextra -std=c11 -g
LDFLAGS ?=

TARGET = minerobot
TARGET_GUI = minerobot_gui

SRC_COMMON = \
	main.c \
	model.c \
	robot.c \
	policy.c \
	controller.c \
	view_cli.c

SRC_GUI = \
	$(SRC_COMMON) \
	view_gui.c

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

all: $(TARGET)

$(TARGET): $(SRC_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_COMMON) $(LDFLAGS)

gui: CFLAGS += $(SDL_CFLAGS) -DWITH_GUI
gui: LDFLAGS += $(SDL_LIBS)
gui: $(SRC_GUI)
	$(CC) $(CFLAGS) -o $(TARGET_GUI) $(SRC_GUI) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_GUI) *.o

.PHONY: all gui clean
```

---

## Example map: `maps/mine1.txt`

```text
10 14
. . . . H . . . . . . O . .
. H . . . . O . H . . . . .
. . . O . . . . . . H . . .
. . H . . H . . O . . . O .
. . . . . . . . . . H . . .
B . . O . . H . . . . . . .
. H . . . . . . O . H . . .
. . . H . O . . . . . . H .
. O . . . . H . . O . . . .
. . . . H . . . . . . O . .
```
