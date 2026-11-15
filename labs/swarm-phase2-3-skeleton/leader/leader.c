/* leader.c — a Dance Captain (leader). Runs as a child process of the center:
 *   stdin  <- messages from the center (ASSIGN, THEME, PLAN, BUSY, BEAT, SHUTDOWN)
 *   stdout -> messages to the center (HELLO, PLAN_REQ, READY, POS, ACK)
 *   stderr -> log
 *
 * Threads:  main = reads stdin;  thinker = commissions plans (Task 2);  K followers (Task 3).
 * Phase 2 changes nothing in the captain except that stdin/stdout are a channel (common/chan.h).
 * Sections marked "given" are complete. Copy YOUR Phase 1 code into the two TODO blocks:
 *   thinker_main (Phase 1 Task 2), follower_main (Phase 1 Task 3, with the battery check).
 *
 * Phase 3:  leader --center HOST:PORT --id L [--adopt]
 *   The link to the center is a TCP connection (the same lines; the first one is HELLO), and the
 *   followers are performer PROCESSES on the radio (UDP, leader/radio.c, Task 3). The follower
 *   threads stay: they still plan each move and claim the cell; move_performer then gets the
 *   performer there. A heartbeat thread (Task 4) tells the center we are alive. --adopt: we
 *   replace a captain that died, so its performers come to us (ADOPT) instead of new ones.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "compat.h"
#include "grid.h"
#include "chan.h"
#include "protocol.h"
#include "queue.h"
#include "radio.h"
#include "swarm.h"
#include "validator.h"

/* ---------------------------------------------------------------------------------- state */

static int      my_id = 0;
static int      battery_beats = -1;              /* --battery N: die (exit 1) when beat N starts */
static int      per_row_locks = 0;               /* --grid row */
static char     who[16];

static Zone     zone;
static int      nf;                              /* number of followers */
static grid_t  *grid;
static compat_barrier_t barrier;
static pthread_t follower_tid[MAX_FOLLOWERS];
static pthread_t thinker_tid;
static queue_t *job_queue;                       /* Theme jobs for the thinker */

/* the channel to the center (common/chan.h): stdin and stdout, which the center connected to
 * two pipes. In Phase 3 it becomes a TCP connection and nothing else in this file changes. */
static chan_t *center_link;

/* Phase 3 */
static const char *center_addr;                  /* --center HOST:PORT: TCP mode */
static int      adopt_mode;                      /* --adopt: the performers come to us */
static int      radio_port = -1;                 /* our radio (UDP); -1: none, followers are threads */
static pthread_t heartbeat_tid;

/* output: every chan_send goes through out_mu (several threads send) */
static pthread_mutex_t out_mu = PTHREAD_MUTEX_INITIALIZER;

static void send_msg(const Msg *m) {
    char line[MAX_LINE];
    int n = proto_format(m, line, sizeof(line));
    if (n < 0) {
        return;
    }
    pthread_mutex_lock(&out_mu);
    chan_send(center_link, line, (size_t)n);
    pthread_mutex_unlock(&out_mu);
}

/* beat clock published by the main thread, consumed by the followers */
static pthread_mutex_t beat_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  beat_cv = PTHREAD_COND_INITIALIZER;
static int             cur_beat = -1;
static int             stop_flag = 0;

/* where each follower is (written by that follower, read by whoever reports) */
static Pos             fpos[MAX_FOLLOWERS];
static unsigned char   fcolor[MAX_FOLLOWERS];
static long            claim_failures;           /* safety violations seen by the grid */

/* Phase 3 (given): get follower idx's performer process to where the grid says it is, for beat b
 * (radio.c). Without a radio (Phases 1 and 2) the follower thread IS the drone: nothing to do.
 * Call it in follower_main after the claim (whether it worked or not), before the second barrier,
 * so all performers fly at the same time and the report comes after they have arrived. */
static long late_performers;
static void move_performer(int idx, int b) {
    pthread_mutex_lock(&beat_mu);
    int over = b < cur_beat;                     /* a restarted captain catches up on old beats */
    pthread_mutex_unlock(&beat_mu);
    if (radio_port > 0 && !over && radio_goto(idx, b, fpos[idx].x, fpos[idx].y, fcolor[idx]) != 0) {
        __atomic_fetch_add(&late_performers, 1, __ATOMIC_RELAXED);
    }
}

/* ---------------------------------------------------------------------- plans (given) */

typedef struct {
    int           ready;
    Theme         theme;
    Pos          *pos;                           /* [len][nf] */
    unsigned char *color;
} Plan;

static Plan            plans[MAX_SECTIONS];
static int             n_plans;
static pthread_mutex_t plan_mu = PTHREAD_MUTEX_INITIALIZER;

/* Store a validated plan. */
static void plan_store(const Theme *t, const Keyframe *kf, int count) {
    int len = t->end_beat - t->start_beat + 1;
    Pos *pos = malloc((size_t)count * sizeof(Pos));
    unsigned char *col = malloc((size_t)count);
    for (int i = 0; i < count; i++) {
        int idx = (kf[i].beat - t->start_beat) * nf + kf[i].follower;
        pos[idx] = (Pos){kf[i].x, kf[i].y};
        col[idx] = kf[i].color;
    }
    (void)len;
    pthread_mutex_lock(&plan_mu);
    if (n_plans < MAX_SECTIONS) {
        plans[n_plans].theme = *t;
        plans[n_plans].pos = pos;
        plans[n_plans].color = col;
        plans[n_plans].ready = 1;                    /* publish last */
        n_plans++;
        pos = NULL;
        col = NULL;
    }
    pthread_mutex_unlock(&plan_mu);
    free(pos);
    free(col);
}

/* Where follower `idx` must be on `beat`, if some stored plan covers that beat. 1 = found. */
static int plan_lookup(int beat, int idx, Pos *p, unsigned char *c) {
    int found = 0;
    pthread_mutex_lock(&plan_mu);
    for (int i = n_plans - 1; i >= 0 && !found; i--) {
        const Theme *t = &plans[i].theme;
        if (plans[i].ready && beat >= t->start_beat && beat <= t->end_beat) {
            int j = (beat - t->start_beat) * nf + idx;
            *p = plans[i].pos[j];
            *c = plans[i].color[j];
            found = 1;
        }
    }
    pthread_mutex_unlock(&plan_mu);
    return found;
}

/* ------------------------------------------------------- center's answers to PLAN_REQ (given) */

typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    int             kind;                        /* 0 none, 1 PLAN, 2 BUSY */
    int             req_id, retry_ms, count;
    Keyframe       *kf;
    int             stop;                        /* set at shutdown: wake the waiter */
} Mailbox;

static Mailbox mailbox = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0, 0, NULL, 0 };

static void mailbox_put(int kind, int req_id, int retry_ms, Keyframe *kf, int count) {
    pthread_mutex_lock(&mailbox.mu);
    free(mailbox.kf);
    mailbox.kind = kind;
    mailbox.req_id = req_id;
    mailbox.retry_ms = retry_ms;
    mailbox.kf = kf;
    mailbox.count = count;
    pthread_cond_signal(&mailbox.cv);
    pthread_mutex_unlock(&mailbox.mu);
}

/* Wait for the next answer. On kind == 1 the caller owns (and must free) *kf. */
static int mailbox_wait(int *req_id, int *retry_ms, Keyframe **kf, int *count) {
    pthread_mutex_lock(&mailbox.mu);
    while (mailbox.kind == 0 && !mailbox.stop) {
        pthread_cond_wait(&mailbox.cv, &mailbox.mu);
    }
    int kind = mailbox.kind;
    *req_id = mailbox.req_id;
    *retry_ms = mailbox.retry_ms;
    *kf = mailbox.kf;
    *count = mailbox.count;
    mailbox.kind = 0;
    mailbox.kf = NULL;
    pthread_mutex_unlock(&mailbox.mu);
    return kind;
}

/* Pause for ms milliseconds (given: use it for the randomized retry delay). */
static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------ Phase 1, Task 2: the thinker */

/* The thinker takes one Theme at a time from job_queue and gets a validated plan for it:
 *
 *   attempt loop (at most 6 attempts per theme):
 *     1. send PLAN_REQ <req_id> <section> <nf> <style_seed>   (req_id: a counter; seed: e.g.
 *        my_id * 1000 + attempt * 7 + a number from the theme, always < 2^31)
 *     2. mailbox_wait(): the center answers with BUSY (kind 2) or PLAN (kind 1)
 *     3. BUSY: sleep_ms(retry_ms + a RANDOM extra between 0 and retry_ms), try again.
 *        (A fixed delay would make every leader retry at the same instant: livelock.)
 *     4. PLAN: validate_plan(theme, &zone, nf, start, kf, count, &err) with `start` = the home
 *        formation (home_position for every follower). Valid: plan_store(), free the
 *        keyframes, send READY <my_id> <section>, done with this theme.
 *        Invalid: log err.msg and err.beat, free the keyframes, try again with a NEW seed.
 *   If every attempt fails: log it and still send READY. The followers will hold their position
 *   (plan_lookup finds no plan), and the show goes on.
 *
 * Stop when queue_pop returns -1 (the queue was closed). */
static void *thinker_main(void *arg) {
    (void)arg;
    /* PHASE 1 (Task 2): copy in your code */
    (void)sleep_ms;
    (void)mailbox_wait;
    (void)plan_store;
    (void)send_msg;
    return NULL;
}

/* ---------------------------------------------------- Phase 1, Task 3: the followers */

/* One follower thread. For every beat b, 0, 1, 2, ... in order (b = last_done + 1; wait on beat_cv
 * until cur_beat >= b, or stop_flag):
 *
 *   target = plan_lookup(b, idx, ...) ? that position : my current position (hold)
 *   if battery_beats >= 0 and b >= battery_beats: simulate a dead battery: _exit(1)
 *
 *   phase 1: grid_release(my current cell);          compat_barrier_wait(&barrier);
 *   phase 2: grid_claim(target)
 *            - failed: count it in claim_failures (a safety violation: log it) and re-claim the
 *              old cell; stay where you were
 *            - ok: fpos[idx] / fcolor[idx] = target
 *                                                     r = compat_barrier_wait(&barrier);
 *   if r == PTHREAD_BARRIER_SERIAL_THREAD (exactly one follower gets it): report the beat:
 *       one POS line per follower (leader id, b, idx, x, y, color), then ACK <my_id> b.
 *
 * Why two barriers? In one beat follower A may move into the cell follower B is leaving. All
 * releases must happen before any claim, or A's claim would fail by accident.
 *
 * PHASE 3: call move_performer(idx, b) after phase 2 (the claim), before the second barrier. */
static void *follower_main(void *arg) {
    int idx = (int)(long)arg;
    (void)idx;
    /* PHASE 1 (Task 3): copy in your code */
    (void)beat_cv;
    (void)beat_mu;
    (void)fcolor;
    (void)fpos;
    (void)claim_failures;
    (void)plan_lookup;
    (void)battery_beats;
    (void)move_performer;                            /* Phase 3 */
    return NULL;
}

/* -------------------------------------------- Phase 3, Task 4: the heartbeat (TCP mode) */

/* TODO (Phase 3, Task 4): send HEARTBEAT <my_id> to the center every 500 ms until stop_flag is
 * set. Without it the center's watchdog declares this captain dead after --heartbeat-ms. Wait with
 * pthread_cond_timedwait on beat_cv (stop_everything broadcasts it; beat_cv uses CLOCK_REALTIME),
 * not with sleep, so the captain exits at once at the end of the show. Why a separate thread, and
 * not "every message counts as a heartbeat"? Think of a PAUSE, and of a captain whose followers
 * are stuck in a deadlock. */
static void *heartbeat_main(void *arg) {
    (void)arg;
    return NULL;
}

/* ------------------------------------------------------------------ main thread (given) */

static void publish_beat(int b) {
    pthread_mutex_lock(&beat_mu);
    cur_beat = b;
    pthread_cond_broadcast(&beat_cv);
    pthread_mutex_unlock(&beat_mu);
}

static void stop_everything(void) {
    pthread_mutex_lock(&beat_mu);
    stop_flag = 1;
    pthread_cond_broadcast(&beat_cv);
    pthread_mutex_unlock(&beat_mu);
    pthread_mutex_lock(&mailbox.mu);
    mailbox.stop = 1;
    pthread_cond_broadcast(&mailbox.cv);
    pthread_mutex_unlock(&mailbox.mu);
    if (job_queue) {
        queue_close(job_queue);
    }
}

/* Read the n keyframe lines that follow a PLAN header. Returns the keyframes (malloc'ed), or NULL
 * if any line was bad; the lines are always consumed. */
static Keyframe *read_plan_body(chan_t *c, int n) {
    Keyframe *kf = malloc((size_t)(n > 0 ? n : 1) * sizeof(Keyframe));
    int bad = kf == NULL;
    char line[MAX_LINE];
    for (int i = 0; i < n; i++) {
        ssize_t r = chan_recv(c, line, sizeof(line), -1);
        if (r == CHAN_CLOSED) {
            free(kf);
            return NULL;
        }
        if (r < 0 || bad || proto_parse_keyframe(line, &kf[i]) != 0) {
            bad = 1;
        }
    }
    if (bad) {
        free(kf);
        return NULL;
    }
    return kf;
}

static void handle_assign(const Msg *m) {
    zone = (Zone){m->x0, m->y0, m->x1, m->y1};
    nf = m->n_followers;
    grid = grid_create(zone.x1 + 1, zone.y1 + 1, per_row_locks);
    compat_barrier_init(&barrier, (unsigned)nf);
    for (int i = 0; i < nf; i++) {
        int hx, hy;
        home_position(&zone, i, &hx, &hy);
        fpos[i] = (Pos){hx, hy};
        fcolor[i] = 15;
        if (grid_claim(grid, hx, hy, i) != 0) {
            log_msg(who, "could not claim home cell for follower %d (grid.c incomplete?)", i);
        }
    }
    job_queue = queue_create(MAX_SECTIONS, sizeof(Theme));
    pthread_create(&thinker_tid, NULL, thinker_main, NULL);
    for (long i = 0; i < nf; i++) {
        pthread_create(&follower_tid[i], NULL, follower_main, (void *)i);
    }
    if (radio_port > 0) {
        radio_launch(nf, adopt_mode, center_addr, my_id);  /* Phase 3: the performer processes */
    }
}

/* Phase 3 (given): connect to the center, with randomized exponential backoff (a --remote
 * captain may be started before the center). */
static chan_t *connect_center(void) {
    char a[300];
    snprintf(a, sizeof(a), "tcp:%s", center_addr);
    int delay = 100;
    for (int attempt = 0; attempt < 8; attempt++) {
        chan_t *c = chan_open(a, CHAN_BOTH);
        if (c) {
            return c;
        }
        sleep_ms(delay + rand() % delay);
        delay = delay < 2000 ? delay * 2 : delay;
    }
    log_msg(who, "cannot reach the center at %s", center_addr);
    return NULL;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            my_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--battery") == 0 && i + 1 < argc) {
            battery_beats = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--grid") == 0 && i + 1 < argc) {
            per_row_locks = strcmp(argv[++i], "row") == 0;
        } else if (strcmp(argv[i], "--center") == 0 && i + 1 < argc) {
            center_addr = argv[++i];                 /* Phase 3 */
        } else if (strcmp(argv[i], "--adopt") == 0) {
            adopt_mode = 1;
        } else {
            fprintf(stderr, "leader: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    snprintf(who, sizeof(who), "leader %d", my_id);
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)getpid());

    if (center_addr) {                               /* Phase 3: TCP to the center, UDP radio */
        radio_port = radio_open();
        if (radio_port <= 0) {
            log_msg(who, "no radio (Phase 3, Task 3 not done?): followers stay threads");
        }
        center_link = connect_center();
        if (!center_link) {
            return 1;
        }
        Msg h = { .type = MSG_HELLO, .leader_id = my_id, .n_followers = 0,
                  .port = radio_port > 0 ? radio_port : 0 };
        send_msg(&h);                                /* the first line says who we are */
        pthread_create(&heartbeat_tid, NULL, heartbeat_main, NULL);
    } else {
        center_link = chan_fd(0, 1);
    }
    char line[MAX_LINE];
    int running = 1, clean = 0;
    while (running) {
        ssize_t r = chan_recv(center_link, line, sizeof(line), -1);
        if (r == CHAN_CLOSED) {
            break;                                   /* the center is gone: exit */
        }
        if (r == CHAN_TOOBIG) {
            log_msg(who, "line too long, skipped");
            continue;
        }
        Msg m;
        if (proto_parse(line, &m) != 0) {
            log_msg(who, "malformed: %s", line);
            continue;
        }
        switch (m.type) {
        case MSG_ASSIGN:
            if (job_queue) {
                log_msg(who, "a second ASSIGN ignored");
                break;
            }
            handle_assign(&m);
            m.type = MSG_HELLO;
            m.leader_id = my_id;
            m.port = radio_port > 0 ? radio_port : 0;
            send_msg(&m);
            break;
        case MSG_ADOPT:                              /* Phase 3: an orphan is coming */
            if (radio_port > 0) {
                radio_adopt(m.follower);
            }
            break;
        case MSG_THEME: {
            Theme t = {0};
            snprintf(t.name, sizeof(t.name), "%s", m.section);
            snprintf(t.theme, sizeof(t.theme), "%s", m.theme);
            snprintf(t.mood, sizeof(t.mood), "%s", m.mood);
            t.start_beat = m.start_beat;
            t.end_beat = m.end_beat;
            if (job_queue && queue_push(job_queue, &t) != 0) {
                log_msg(who, "could not queue theme %s", t.name);
            }
            break;
        }
        case MSG_PLAN: {
            Keyframe *kf = read_plan_body(center_link, m.n);
            if (kf) {
                mailbox_put(1, m.req_id, 0, kf, m.n);
            } else {
                log_msg(who, "bad PLAN body for request %d", m.req_id);
                mailbox_put(2, m.req_id, 20, NULL, 0);     /* treat it like BUSY: ask again */
            }
            break;
        }
        case MSG_BUSY:
            mailbox_put(2, m.req_id, m.retry_ms, NULL, 0);
            break;
        case MSG_BEAT:
            publish_beat(m.beat);
            break;
        case MSG_SHUTDOWN:
            running = 0;
            clean = 1;
            break;
        default:
            log_msg(who, "ignoring: %s", line);
            break;
        }
    }
    stop_everything();
    if (!clean) {
        _exit(1);                                    /* the center vanished: do not wait for anything */
    }
    if (job_queue) {
        pthread_join(thinker_tid, NULL);
        for (int i = 0; i < nf; i++) {
            pthread_join(follower_tid[i], NULL);
        }
    }
    if (center_addr) {
        pthread_join(heartbeat_tid, NULL);           /* it sees stop_flag */
    }
    if (radio_port > 0) {
        radio_close();                               /* LAND the performers, reap ours */
    }
    if (claim_failures) {
        log_msg(who, "%ld claim failure(s)", claim_failures);
    }
    if (late_performers) {
        log_msg(who, "%ld late performer move(s)", late_performers);
    }
    chan_close(center_link);
    return 0;
}
