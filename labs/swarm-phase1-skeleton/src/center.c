/* center.c — the Orchestrator Center (Artistic Director).
 *
 * Starts one leader process per zone (pipes on the leader's stdin/stdout), tells each its zone and
 * the theme of each section, runs the beat clock, and hosts the choreographer: a thread pool that
 * answers PLAN_REQ messages.
 *
 * Most of this file is given. YOUR code is in the TODO blocks:
 *   Task 2:  handle_plan_req and serve_request  (request queue, KV pool, semaphore, PLAN/BUSY)
 *   Task 5:  on_leader_exit                      (notice a dead captain, restart it)
 *
 * Thread map:   main     — clock loop: THEME, BEAT, waits for READY and ACK
 *               readers  — one per leader incarnation: parses that leader's stdout
 *               workers  — the planner pool: pop a Request, call plan_section, reply with PLAN
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "kvpool.h"
#include "line_io.h"
#include "metrics.h"
#include "planner.h"
#include "protocol.h"
#include "queue.h"
#include "swarm.h"

/* ------------------------------------------------------------------------------ config */

typedef struct {
    const char *score_path;
    int   n_leaders, followers;
    int   tempo_override;                 /* -1: use the score's tempo; 0: lockstep, no waiting */
    int   think_ms, flaky_pct;
    int   pool_blocks, block_size;        /* the KV pool (Lab 6) */
    int   workers, queue_cap, max_inflight;
    int   restart;                        /* restart a dead captain? */
    int   ascii_every;                    /* print the stage every N beats (0 = never) */
    int   timeout_s;                      /* watchdog: abort with exit status 3 */
    int   battery_leader, battery_beats;  /* --battery L:B  -> leader L dies at beat B (first life) */
    const char *grid_mode;                /* "global" or "row" (passed to the leaders) */
} Config;

static Config cfg = {
    "data/scores/short.score", 4, 16, -1, 20, 0, 256, 1024, 2, 8, 2, 1, 0, 60, -1, -1, "global"
};

/* ------------------------------------------------------------------------------- state */

typedef struct {
    int             id, inc;              /* inc: incarnation, bumped at every (re)start */
    pid_t           pid;
    int             to_fd, from_fd;
    int             alive;                /* a process is running */
    int             gone;                 /* died and will not be restarted */
    Zone            zone;
    pthread_mutex_t wlock;                /* serializes writes to to_fd */
    pthread_t       reader;
} Leader;

typedef struct { int leader, req_id, section, n; unsigned seed; } Request;

static Score  score;
static Image  target;
static Leader leaders[MAX_LEADERS];

/* everything below is protected by st_mu */
static pthread_mutex_t st_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  st_cv;                          /* initialized with a monotonic clock */
static unsigned        ready_mask[MAX_LEADERS];        /* bit s: READY received for section s */
static int             acked[MAX_LEADERS];             /* last beat acknowledged */
static Theme           sent_theme[MAX_LEADERS][MAX_SECTIONS];
static Pos             spos[MAX_LEADERS][MAX_FOLLOWERS];
static unsigned char   scol[MAX_LEADERS][MAX_FOLLOWERS];
static Pos             prevpos[MAX_LEADERS][MAX_FOLLOWERS];
static int             skip_speed[MAX_LEADERS];        /* a restarted leader jumps back to home */
static long            busy_count, late_count, restart_count;
static int             cur_beat;                       /* the beat being run */

static int shutting_down;                              /* set by main under st_mu */

/* the choreographer */
static queue_t        *req_queue;
static KVPool         *pool;
static pthread_mutex_t pool_mu = PTHREAD_MUTEX_INITIALIZER;   /* NOT used yet: see Task 2 */
static sem_t           inflight;                              /* NOT used yet: see Task 2 */

/* ------------------------------------------------------------------------ small helpers */

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void leader_send_raw(int l, const char *buf, size_t len) {
    Leader *ld = &leaders[l];
    pthread_mutex_lock(&ld->wlock);
    if (ld->alive) {
        send_all(ld->to_fd, buf, len);            /* EPIPE is ignored: the reader will see EOF */
    }
    pthread_mutex_unlock(&ld->wlock);
}

static void leader_send(int l, const Msg *m) {
    char line[MAX_LINE + 1];
    int n = proto_format(m, line, sizeof(line) - 1);
    if (n < 0) {
        return;
    }
    line[n++] = '\n';
    leader_send_raw(l, line, (size_t)n);
}

static void send_busy(int l, int req_id, int retry_ms) {
    Msg m = { .type = MSG_BUSY, .req_id = req_id, .retry_ms = retry_ms };
    leader_send(l, &m);
    pthread_mutex_lock(&st_mu);
    busy_count++;
    pthread_mutex_unlock(&st_mu);
}

/* PLAN header and all keyframe lines in ONE write, so nothing can interleave with them. */
static void send_plan(int l, int req_id, const Keyframe *kf, int count) {
    size_t cap = (size_t)count * 40 + 64;
    char *buf = malloc(cap);
    if (!buf) {
        return;
    }
    size_t off = 0;
    Msg m = { .type = MSG_PLAN, .req_id = req_id, .n = count };
    int n = proto_format(&m, buf + off, cap - off);
    buf[off + (size_t)n] = '\n';
    off += (size_t)n + 1;
    for (int i = 0; i < count; i++) {
        n = proto_format_keyframe(&kf[i], buf + off, cap - off);
        buf[off + (size_t)n] = '\n';
        off += (size_t)n + 1;
    }
    leader_send_raw(l, buf, off);
    free(buf);
}

static void send_theme(int l, int section, int start_beat) {
    Theme t = score.sections[section];
    t.start_beat = start_beat;                    /* shortened when a captain is restarted */
    pthread_mutex_lock(&st_mu);
    sent_theme[l][section] = t;
    pthread_mutex_unlock(&st_mu);
    Msg m = { .type = MSG_THEME, .start_beat = t.start_beat, .end_beat = t.end_beat };
    snprintf(m.section, sizeof(m.section), "%s", t.name);
    snprintf(m.theme, sizeof(m.theme), "%s", t.theme);
    snprintf(m.mood, sizeof(m.mood), "%s", t.mood);
    leader_send(l, &m);
}

static void send_assign(int l) {
    Msg m = { .type = MSG_ASSIGN, .leader_id = l, .x0 = leaders[l].zone.x0, .y0 = leaders[l].zone.y0,
              .x1 = leaders[l].zone.x1, .y1 = leaders[l].zone.y1, .n_followers = cfg.followers };
    leader_send(l, &m);
}

static int section_by_name(const char *name) {
    for (int i = 0; i < score.n_sections; i++) {
        if (strcmp(score.sections[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- leader processes (given) */

static void *reader_main(void *arg);

/* fork + exec ./leader with the same pipe/dup2 pattern as Lab 4. */
static int spawn_leader(int l, int battery) {
    Leader *ld = &leaders[l];
    int to_child[2], from_child[2];
    if (pipe(to_child) < 0 || pipe(from_child) < 0) {
        perror("pipe");
        return -1;
    }
    int other_fds[2 * MAX_LEADERS], n_other = 0;       /* snapshot: another thread may restart a leader */
    pthread_mutex_lock(&st_mu);
    for (int i = 0; i < MAX_LEADERS; i++) {
        if (i != l && leaders[i].alive) {
            other_fds[n_other++] = leaders[i].to_fd;
            other_fds[n_other++] = leaders[i].from_fd;
        }
    }
    pthread_mutex_unlock(&st_mu);
    char id[16], bat[16];
    snprintf(id, sizeof(id), "%d", l);
    snprintf(bat, sizeof(bat), "%d", battery);
    const char *bin = getenv("LEADER_BIN") ? getenv("LEADER_BIN") : "./leader";   /* make tsan */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        for (int i = 0; i < n_other; i++) {              /* do not inherit the other leaders' pipes */
            close(other_fds[i]);
        }
        if (battery >= 0) {
            execl(bin, "leader", "--id", id, "--grid", cfg.grid_mode, "--battery", bat,
                  (char *)NULL);
        } else {
            execl(bin, "leader", "--id", id, "--grid", cfg.grid_mode, (char *)NULL);
        }
        perror("exec ./leader");
        _exit(127);                                      /* _exit: do not flush the parent's buffers */
    }
    close(to_child[0]);
    close(from_child[1]);
    pthread_mutex_lock(&ld->wlock);                      /* wlock, then st_mu: the lock order */
    pthread_mutex_lock(&st_mu);
    ld->pid = pid;
    ld->to_fd = to_child[1];
    ld->from_fd = from_child[0];
    ld->inc++;
    ld->alive = 1;
    ready_mask[l] = 0;
    acked[l] = cur_beat - 1;
    pthread_mutex_unlock(&st_mu);
    pthread_mutex_unlock(&ld->wlock);
    int *ra = malloc(2 * sizeof(int));
    ra[0] = l;
    ra[1] = ld->inc;
    int restarted = ld->inc > 1;
    pthread_t old_reader = ld->reader;
    pthread_create(&ld->reader, NULL, reader_main, ra);
    if (restarted) {
        pthread_detach(old_reader);                      /* the old reader (our caller) ends by itself */
    }
    return 0;
}

/* ---------------------------------------------------------------- Task 2: the choreographer */

/* Called by a reader thread for every PLAN_REQ from leader l.
 *
 * TODO (Task 2): build a Request {leader, req_id, section (use section_by_name), n, seed} and
 * queue_try_push it onto req_queue. If the queue is full (or the section is unknown), answer
 * immediately with send_busy(l, m->req_id, 50) instead of blocking the reader thread. */
static void handle_plan_req(int l, const Msg *m) {
    (void)l;
    (void)m;
    (void)section_by_name;
    (void)send_busy;
}

/* Called by a worker thread for one Request. `arena` is that worker's private arena.
 *
 * TODO (Task 2), in this order:
 *   1. sem_wait(&inflight)                      at most cfg.max_inflight planner calls at once
 *   2. look up the theme and zone:  sent_theme[r->leader][r->section] (under st_mu) and
 *      leaders[r->leader].zone; count = r->n * (end_beat - start_beat + 1)
 *   3. take the request's KV blocks: need = ceil(count * sizeof(Keyframe) / block_size), as many
 *      blocks as the "KV cache" of this request. The Lab 6 pool is NOT thread-safe: protect every
 *      kv_blocks_alloc / kv_blocks_free with pool_mu. If kv_blocks_alloc fails (-1), release the
 *      semaphore and answer send_busy(..., 50): the pool is the backpressure
 *   4. arena_reset(arena); Keyframe *kf = arena_alloc(arena, count * sizeof(Keyframe));
 *      touch the blocks (memset each to the seed) to pretend they hold KV state
 *   5. n = plan_section(&theme, &zone, &target, r->n, r->seed, kf, count)  (this sleeps: it is
 *      the slow "model"); n < 0 -> send_busy and clean up
 *   6. send_plan(r->leader, r->req_id, kf, n)
 *   7. give the blocks back (under pool_mu) and sem_post(&inflight)
 * Every exit path must release what it took, or the pool and the semaphore leak. */
static void serve_request(const Request *r, Arena *arena) {
    (void)r;
    (void)arena;
    (void)send_plan;
    (void)pool_mu;
    (void)inflight;
    (void)pool;
}

static void *worker_main(void *arg) {
    (void)arg;
    Arena *arena = arena_create((size_t)MAX_FOLLOWERS * MAX_BEATS * sizeof(Keyframe) + 64);
    Request r;
    while (queue_pop(req_queue, &r) == 0) {
        serve_request(&r, arena);
    }
    arena_destroy(arena);
    return NULL;
}

/* ------------------------------------------------------------ Task 5: a captain went dark */

/* Reap the process, close its pipes and mark it not alive. Takes wlock first, then st_mu (the lock
 * order), because senders read `alive` and `to_fd` under wlock and the clock loop under st_mu. */
static void mark_dead(Leader *ld) {
    waitpid(ld->pid, NULL, 0);
    pthread_mutex_lock(&ld->wlock);
    pthread_mutex_lock(&st_mu);
    close(ld->to_fd);
    close(ld->from_fd);
    ld->alive = 0;
    pthread_cond_broadcast(&st_cv);
    pthread_mutex_unlock(&st_mu);
    pthread_mutex_unlock(&ld->wlock);
}

/* Called when the reader of leader `l` (incarnation `inc`) reaches EOF: the process closed its
 * stdout. At the end of the show that is normal; in the middle of the show it means the captain
 * died (for example --battery).
 *
 * TODO (Task 5), for an UNEXPECTED death (not shutting_down):
 *   - waitpid(pid) to reap it (no zombie!) and close both pipe fds
 *   - print "captain <l> went dark at beat <cur_beat>" to stderr
 *   - mark it not alive (under st_mu) and broadcast st_cv so the clock loop stops waiting for it
 *   - if cfg.restart: restart_count++, spawn_leader(l, -1) (no battery this time), then send_assign
 *     and send_theme(l, section_at(cur_beat + 1), cur_beat + 1) so the new captain resumes from
 *     the next beat, and send_theme for the following section too if it exists; also set
 *     skip_speed[l] = 1 (the new captain's drones start at home)
 *   - otherwise leaders[l].gone = 1
 * Think about what must happen to st_cv waiters, and about a death while you are restarting. */
static void on_leader_exit(int l, int inc) {
    Leader *ld = &leaders[l];
    pthread_mutex_lock(&st_mu);
    int stale = inc != ld->inc;
    int closing = shutting_down;
    pthread_mutex_unlock(&st_mu);
    if (stale) {
        return;
    }
    if (closing) {
        mark_dead(ld);
        return;
    }
    /* TODO (Task 5) */
    (void)restart_count;
    (void)spawn_leader;
    (void)send_assign;
    (void)send_theme;
}

/* ------------------------------------------------------------------ reader threads (given) */

static void *reader_main(void *arg) {
    int l = ((int *)arg)[0], inc = ((int *)arg)[1];
    free(arg);
    LineReader lr;
    lr_init(&lr, leaders[l].from_fd);
    char line[MAX_LINE];
    for (;;) {
        int r = lr_read_line(&lr, line, sizeof(line));
        if (r == 0 || r == -1) {
            break;
        }
        if (r == -2) {
            log_msg("center", "line from leader %d too long, skipped", l);
            continue;
        }
        Msg m;
        if (proto_parse(line, &m) != 0) {
            log_msg("center", "malformed from leader %d: %s", l, line);
            continue;
        }
        switch (m.type) {
        case MSG_PLAN_REQ:
            handle_plan_req(l, &m);
            break;
        case MSG_READY: {
            int s = section_by_name(m.section);
            pthread_mutex_lock(&st_mu);
            if (s >= 0 && inc == leaders[l].inc) {
                ready_mask[l] |= 1u << s;
            }
            pthread_cond_broadcast(&st_cv);
            pthread_mutex_unlock(&st_mu);
            break;
        }
        case MSG_POS:
            pthread_mutex_lock(&st_mu);
            if (inc == leaders[l].inc && m.follower < cfg.followers) {
                spos[l][m.follower] = (Pos){m.x, m.y};
                scol[l][m.follower] = (unsigned char)m.color;
            }
            pthread_mutex_unlock(&st_mu);
            break;
        case MSG_ACK:
            pthread_mutex_lock(&st_mu);
            if (inc == leaders[l].inc && m.beat > acked[l]) {
                acked[l] = m.beat;
            }
            pthread_cond_broadcast(&st_cv);
            pthread_mutex_unlock(&st_mu);
            break;
        case MSG_HELLO:
        case MSG_HEARTBEAT:
        case MSG_DONE:
            break;
        default:
            log_msg("center", "ignoring from leader %d: %s", l, line);
            break;
        }
    }
    on_leader_exit(l, inc);
    return NULL;
}

/* ------------------------------------------------------------- the clock loop (given) */

/* Wait on st_cv until pred() holds. deadline_ms < 0: wait forever. Returns 1 if pred held, 0 on
 * timeout. Call with st_mu held. */
static int wait_for(int (*pred)(int), int arg, long deadline_ms) {
    while (!pred(arg)) {
        if (deadline_ms < 0) {
            pthread_cond_wait(&st_cv, &st_mu);
        } else {
            long left = deadline_ms - now_ms();
            if (left <= 0) {
                return 0;
            }
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += left / 1000;
            ts.tv_nsec += (left % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            if (pthread_cond_timedwait(&st_cv, &st_mu, &ts) == ETIMEDOUT && !pred(arg)) {
                return 0;
            }
        }
    }
    return 1;
}

static int all_ready(int section) {
    for (int l = 0; l < cfg.n_leaders; l++) {
        if (!leaders[l].gone && !(ready_mask[l] & (1u << section))) {
            return 0;
        }
    }
    return 1;
}

static int all_acked(int beat) {
    for (int l = 0; l < cfg.n_leaders; l++) {
        if (!leaders[l].gone && (!leaders[l].alive || acked[l] < beat)) {
            if (!leaders[l].alive && !leaders[l].gone) {
                return 0;                 /* restarting: wait for the new captain */
            }
            return 0;
        }
    }
    return 1;
}

static void print_stage(int beat) {
    char *grid = malloc((size_t)score.stage_w * (size_t)score.stage_h);
    memset(grid, '.', (size_t)score.stage_w * (size_t)score.stage_h);
    pthread_mutex_lock(&st_mu);
    for (int l = 0; l < cfg.n_leaders; l++) {
        for (int f = 0; f < cfg.followers; f++) {
            Pos p = spos[l][f];
            if (p.x >= 0 && p.x < score.stage_w && p.y >= 0 && p.y < score.stage_h) {
                grid[p.y * score.stage_w + p.x] = "0123456789abcdef"[scol[l][f] & 15];
            }
        }
    }
    pthread_mutex_unlock(&st_mu);
    printf("--- beat %d ---\n", beat);
    for (int y = 0; y < score.stage_h; y++) {
        printf("%.*s\n", score.stage_w, grid + y * score.stage_w);
    }
    free(grid);
}

static void on_alarm(int sig) {
    (void)sig;
    static const char msg[] = "center: TIMEOUT (stuck? possible deadlock)\n";
    ssize_t r = write(2, msg, sizeof(msg) - 1);
    (void)r;
    _exit(3);
}

static int parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int has = i + 1 < argc;
        if (!strcmp(a, "--score") && has)             cfg.score_path = argv[++i];
        else if (!strcmp(a, "--leaders") && has)      cfg.n_leaders = atoi(argv[++i]);
        else if (!strcmp(a, "--followers") && has)    cfg.followers = atoi(argv[++i]);
        else if (!strcmp(a, "--tempo") && has)        cfg.tempo_override = atoi(argv[++i]);
        else if (!strcmp(a, "--think-ms") && has)     cfg.think_ms = atoi(argv[++i]);
        else if (!strcmp(a, "--flaky") && has)        cfg.flaky_pct = atoi(argv[++i]);
        else if (!strcmp(a, "--pool-blocks") && has)  cfg.pool_blocks = atoi(argv[++i]);
        else if (!strcmp(a, "--workers") && has)      cfg.workers = atoi(argv[++i]);
        else if (!strcmp(a, "--queue-cap") && has)    cfg.queue_cap = atoi(argv[++i]);
        else if (!strcmp(a, "--max-inflight") && has) cfg.max_inflight = atoi(argv[++i]);
        else if (!strcmp(a, "--no-restart"))          cfg.restart = 0;
        else if (!strcmp(a, "--ascii") && has)        cfg.ascii_every = atoi(argv[++i]);
        else if (!strcmp(a, "--timeout") && has)      cfg.timeout_s = atoi(argv[++i]);
        else if (!strcmp(a, "--grid") && has)         cfg.grid_mode = argv[++i];
        else if (!strcmp(a, "--battery") && has) {
            if (sscanf(argv[++i], "%d:%d", &cfg.battery_leader, &cfg.battery_beats) != 2) {
                return -1;
            }
        } else {
            fprintf(stderr, "center: bad argument %s\n", a);
            return -1;
        }
    }
    if (cfg.n_leaders < 1 || cfg.n_leaders > MAX_LEADERS || cfg.followers < 1 ||
        cfg.followers > MAX_FOLLOWERS || cfg.workers < 1 || cfg.queue_cap < 1 ||
        cfg.max_inflight < 1 || cfg.pool_blocks < 1 || cfg.flaky_pct < 0 || cfg.flaky_pct > 100 ||
        (strcmp(cfg.grid_mode, "global") && strcmp(cfg.grid_mode, "row"))) {
        fprintf(stderr, "center: argument out of range\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr,
                "usage: center [--score F] [--leaders N] [--followers K] [--tempo BPM|0] [--think-ms MS]\n"
                "              [--flaky PCT] [--pool-blocks N] [--workers N] [--queue-cap N]\n"
                "              [--max-inflight N] [--grid global|row] [--battery L:B] [--no-restart]\n"
                "              [--ascii EVERY] [--timeout SEC]\n");
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, on_alarm);
    alarm((unsigned)cfg.timeout_s);

    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&st_cv, &ca);

    if (score_load(cfg.score_path, &score) != 0) {
        return 1;
    }
    if (score.image_path[0] && image_load(score.image_path, &target) != 0) {
        return 1;
    }
    int tempo = cfg.tempo_override >= 0 ? cfg.tempo_override : score.tempo_bpm;
    int period_ms = tempo > 0 ? 60000 / tempo : 0;
    int last_beat = score_last_beat(&score);
    int ntot = cfg.n_leaders * cfg.followers;

    PlannerOpts po = { cfg.think_ms, cfg.flaky_pct };
    planner_init(&po);
    pool = kv_pool_create((size_t)cfg.block_size, (size_t)cfg.pool_blocks);
    req_queue = queue_create((size_t)cfg.queue_cap, sizeof(Request));
    sem_init(&inflight, 0, (unsigned)cfg.max_inflight);
    if (!pool || !req_queue) {
        fprintf(stderr, "center: pool or queue missing (implement queue.c; check the pool)\n");
        return 1;
    }
    pthread_t workers[16];
    int nworkers = cfg.workers > 16 ? 16 : cfg.workers;
    for (int i = 0; i < nworkers; i++) {
        pthread_create(&workers[i], NULL, worker_main, NULL);
    }

    cur_beat = 0;
    for (int l = 0; l < cfg.n_leaders; l++) {
        pthread_mutex_init(&leaders[l].wlock, NULL);
        leaders[l].id = l;
        zone_split(score.stage_w, score.stage_h, cfg.n_leaders, l, &leaders[l].zone);
        for (int f = 0; f < cfg.followers; f++) {
            home_position(&leaders[l].zone, f, &spos[l][f].x, &spos[l][f].y);
            prevpos[l][f] = spos[l][f];
            scol[l][f] = 15;
        }
        if (spawn_leader(l, l == cfg.battery_leader ? cfg.battery_beats : -1) != 0) {
            return 1;
        }
        send_assign(l);
        send_theme(l, 0, score.sections[0].start_beat);
    }

    long t_start = now_ms();
    long t_beat = t_start;
    long collisions = 0, speed_violations = 0;
    for (int b = 0; b <= last_beat; b++) {
        int s = score_section_at(&score, b);
        long deadline = period_ms ? t_beat : -1;

        pthread_mutex_lock(&st_mu);
        cur_beat = b;
        int ready = wait_for(all_ready, s, deadline);
        if (!ready) {
            late_count++;                                /* real time: the show goes on, late */
        }
        pthread_mutex_unlock(&st_mu);
        if (b == score.sections[s].start_beat && s + 1 < score.n_sections) {
            for (int l = 0; l < cfg.n_leaders; l++) {
                if (!leaders[l].gone) {
                    send_theme(l, s + 1, score.sections[s + 1].start_beat);
                }
            }
        }

        Msg bm = { .type = MSG_BEAT, .beat = b };
        for (int l = 0; l < cfg.n_leaders; l++) {
            leader_send(l, &bm);
        }

        pthread_mutex_lock(&st_mu);
        long ack_deadline = period_ms ? t_beat + period_ms : -1;
        if (!wait_for(all_acked, b, ack_deadline)) {
            late_count++;
            wait_for(all_acked, b, -1);                  /* keep the beats in lockstep */
        }
        Pos flat[MAX_LEADERS * MAX_FOLLOWERS], prev[MAX_LEADERS * MAX_FOLLOWERS];
        int n = 0;
        for (int l = 0; l < cfg.n_leaders; l++) {
            for (int f = 0; f < cfg.followers; f++) {
                flat[n] = spos[l][f];
                prev[n] = skip_speed[l] ? spos[l][f] : prevpos[l][f];
                prevpos[l][f] = spos[l][f];
                n++;
            }
            skip_speed[l] = 0;
        }
        collisions += audit_collisions(flat, n);
        speed_violations += audit_speed(prev, flat, n);
        pthread_mutex_unlock(&st_mu);

        if (cfg.ascii_every > 0 && b % cfg.ascii_every == 0) {
            print_stage(b);
        }
        if (period_ms) {
            t_beat += period_ms;
            long left = t_beat - now_ms();
            if (left > 0) {
                struct timespec ts = { left / 1000, (left % 1000) * 1000000L };
                nanosleep(&ts, NULL);
            }
        }
    }

    /* final score */
    double acc = -1.0;
    if (strcmp(score.sections[score.n_sections - 1].theme, "mosaic") == 0) {
        Pos flat[MAX_LEADERS * MAX_FOLLOWERS];
        unsigned char col[MAX_LEADERS * MAX_FOLLOWERS];
        int n = 0;
        pthread_mutex_lock(&st_mu);
        for (int l = 0; l < cfg.n_leaders; l++) {
            for (int f = 0; f < cfg.followers; f++, n++) {
                flat[n] = spos[l][f];
                col[n] = scol[l][f];
            }
        }
        pthread_mutex_unlock(&st_mu);
        acc = mosaic_accuracy(&target, flat, col, n);
    }

    /* shutdown */
    pthread_mutex_lock(&st_mu);
    shutting_down = 1;
    pthread_mutex_unlock(&st_mu);
    Msg sm = { .type = MSG_SHUTDOWN };
    for (int l = 0; l < cfg.n_leaders; l++) {
        leader_send(l, &sm);
    }
    for (int l = 0; l < cfg.n_leaders; l++) {
        pthread_join(leaders[l].reader, NULL);
    }
    queue_close(req_queue);
    for (int i = 0; i < nworkers; i++) {
        pthread_join(workers[i], NULL);
    }
    double elapsed = (double)(now_ms() - t_start) / 1000.0;
    printf("RESULT beats=%d drones=%d collisions=%ld speed_violations=%ld mosaic_accuracy=%.3f "
           "busy=%ld late=%ld restarts=%ld elapsed=%.2f\n",
           last_beat + 1, ntot, collisions, speed_violations, acc, busy_count, late_count,
           restart_count, elapsed);
    queue_destroy(req_queue);
    kv_pool_destroy(pool);
    image_free(&target);
    return (collisions == 0 && speed_violations == 0) ? 0 : 2;
}
