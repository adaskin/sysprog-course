/* conn.c -- YOUR FILE (TODO 3). Glue between the framing and the exam service. */
#include "server.h"

#include <string.h>

/* TODO 3: see server.h. Hints:
 *   char line[MAX_LINE + 1], out[OUT_MAX]; int close_after = 0;
 *   while ((r = lb_next_line(&c->lb, line, sizeof(line))) == 1) { ... }
 * Remember: after ONE read the buffer can hold SEVERAL requests (pipelining). */
int conn_serve(conn_t *c) {
    (void)c;
    /* TODO */
    return 0;
}
