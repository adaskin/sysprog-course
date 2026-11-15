/* view_nogui.c — stands in for view_gui.c when the Makefile finds no GUI toolkit (given).
 * Install one (sudo apt install libsdl2-dev) or set GUI_PKG in the Makefile, then `make` again. */
#include <stdio.h>
#include "view.h"

static int nogui_open(const ViewOptions *o) {
    (void)o;
    fprintf(stderr, "viewer: built without a GUI toolkit (sudo apt install libsdl2-dev, then make "
                    "clean && make). Try --view term.\n");
    return -1;
}

static void nogui_draw(const Frame *f) {
    (void)f;
}

static int nogui_input(char *cmd, size_t n, int wait_ms) {
    (void)cmd;
    (void)n;
    (void)wait_ms;
    return -1;
}

static int nogui_close(void) {
    return 0;
}

const View view_gui = { "gui", 1, nogui_open, nogui_draw, nogui_input, nogui_close };
