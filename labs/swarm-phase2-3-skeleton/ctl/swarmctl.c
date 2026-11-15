/* swarmctl.c — the operator's command-line tool (given).
 *
 *   swarmctl [--name NAME] pause | resume | tempo BPM | kill ID | stop      commands to the center
 *   swarmctl [--name NAME] raw 'ANY LINE'                                  send a line as it is
 *   swarmctl [--name NAME] status                                          the newest frame, once
 *   swarmctl [--name NAME] watch                                           one line per frame
 *   swarmctl --center HOST:PORT ...                                        Phase 3: the same, over TCP
 *
 * Commands go over the control channel (fifo:NAME, Task 3); status and watch read the stage bus
 * (shm:NAME, Task 1), the same frames a viewer gets. Like kubectl for a cluster: the control plane
 * takes orders, the data plane shows the state, and the tool uses both. With --center (Phase 3)
 * both go over one TCP connection each: OPERATE (the center answers OK or ERR) and WATCH.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "chan.h"
#include "frame.h"
#include "frame_text.h"
#include "swarm.h"

static const char *center;                           /* --center HOST:PORT (Phase 3) */

static void print_status(const Frame *f) {
    char alive[16] = {0};
    for (int l = 0; l < f->n_leaders && l < 15; l++) {
        alive[l] = (f->alive_mask >> l & 1) ? '1' : '0';
    }
    printf("beat=%d last_beat=%d section=%s theme=%s tempo=%d paused=%d finished=%d alive=%s "
           "collisions=%u speed_violations=%u busy=%u late=%u restarts=%u\n",
           f->beat, f->last_beat, f->section_name, f->theme, f->tempo_bpm, f->paused, f->finished,
           alive, f->collisions, f->speed_violations, f->busy, f->late, f->restarts);
    fflush(stdout);
}

static int watch(const char *name, int once) {
    char addr[300];
    if (center) {
        snprintf(addr, sizeof(addr), "tcp:%s", center);
    } else {
        snprintf(addr, sizeof(addr), "shm:%s", name);
    }
    chan_t *bus = chan_open(addr, center ? CHAN_BOTH : CHAN_RECV);
    if (bus && center && chan_send(bus, "WATCH", 5) != 0) {
        chan_close(bus);
        bus = NULL;
    }
    if (!bus) {
        fprintf(stderr, "swarmctl: no show '%s' to watch\n", center ? center : name);
        return 1;
    }
    static Frame f;
    int got = 0;
    for (;;) {
        ssize_t r;
        if (center) {
            r = frame_recv_text(bus, &f, 2000);
            r = r == 0 ? (ssize_t)sizeof(f) : r;
        } else {
            r = chan_recv(bus, &f, sizeof(f), 2000);
        }
        if (r == (ssize_t)sizeof(f)) {
            print_status(&f);
            got = 1;
            if (once) {
                break;
            }
        } else if (r == CHAN_CLOSED || (once && r == CHAN_TIMEOUT)) {
            break;
        }
    }
    chan_close(bus);
    return got ? 0 : 1;
}

int main(int argc, char **argv) {
    static char name[32];
    snprintf(name, sizeof(name), "swarm-%u", (unsigned)getuid());
    const char *show = name;
    int i = 1;
    if (i + 1 < argc && !strcmp(argv[i], "--name")) {
        show = argv[i + 1];
        i += 2;
    } else if (i + 1 < argc && !strcmp(argv[i], "--center")) {
        center = argv[i + 1];
        i += 2;
    }
    if (i >= argc) {
        fprintf(stderr, "usage: swarmctl [--name NAME | --center HOST:PORT] pause|resume|tempo BPM|"
                        "kill ID|stop|raw LINE|status|watch\n");
        return 2;
    }
    const char *verb = argv[i];
    signal(SIGPIPE, SIG_IGN);                        /* the center may close the channel under us */
    if (!strcmp(verb, "status")) {
        return watch(show, 1);
    }
    if (!strcmp(verb, "watch")) {
        return watch(show, 0);
    }
    char line[MAX_LINE];
    if (!strcmp(verb, "raw") && i + 1 < argc) {
        snprintf(line, sizeof(line), "%s", argv[i + 1]);
    } else {
        size_t o = 0;                                /* "tempo 90" -> "TEMPO 90" */
        for (int k = i; k < argc && o + 1 < sizeof(line); k++) {
            for (const char *p = argv[k]; *p && o + 2 < sizeof(line); p++) {
                line[o++] = (char)toupper((unsigned char)*p);
            }
            if (k + 1 < argc) {
                line[o++] = ' ';
            }
        }
        line[o] = '\0';
    }
    if (center) {                                    /* Phase 3: OPERATE, then the command */
        char addr[300], reply[MAX_LINE];
        snprintf(addr, sizeof(addr), "tcp:%s", center);
        chan_t *c = chan_open(addr, CHAN_BOTH);
        if (!c) {
            fprintf(stderr, "swarmctl: no center at %s\n", center);
            return 1;
        }
        ssize_t r = chan_send(c, "OPERATE", 7) == 0 && chan_send(c, line, strlen(line)) == 0
                        ? chan_recv(c, reply, sizeof(reply), 2000) : CHAN_CLOSED;
        chan_close(c);
        if (r < 0) {
            fprintf(stderr, "swarmctl: no answer from the center\n");
            return 1;
        }
        printf("%s\n", reply);
        return strncmp(reply, "OK", 2) == 0 ? 0 : 1;
    }
    char addr[128];
    ctl_addr(show, addr, sizeof(addr));
    chan_t *c = chan_open(addr, CHAN_SEND);
    if (!c) {
        fprintf(stderr, "swarmctl: no show '%s' is listening on %s\n", show, addr);
        return 1;
    }
    int r = chan_send(c, line, strlen(line));
    chan_close(c);
    if (r != 0) {
        fprintf(stderr, "swarmctl: send failed\n");
        return 1;
    }
    return 0;
}
