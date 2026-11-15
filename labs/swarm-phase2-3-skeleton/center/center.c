/* center.c — the Orchestrator Center (Artistic Director): the model of the show.
 *
 * Starts one leader process per zone (pipes on the leader's stdin/stdout), tells each its zone and
 * the theme of each section, runs the beat clock, and hosts the choreographer: a thread pool that
 * answers PLAN_REQ messages. After every beat it audits the stage and PUBLISHES it as a Frame:
 * to the stage bus (shared memory, for any number of viewers) and to the flight recorder (a file).
 * The center never draws anything: viewers are separate programs (viewer/).
 *
 * Every link is a channel (common/chan.h): the leaders (chan_fd over two pipes), the stage bus
 * ("shm:NAME"), the flight recorder ("file:PATH") and the control channel ("fifo:NAME").
 *
 * Phase 3 (--port P): the center is also a TCP server (center/net.c). The captains connect back
 * to it ("leader --center 127.0.0.1:P"; a --remote captain from another machine), viewers and
 * operators connect to the same port, and orphaned performers ask it for a new captain. The
 * Phase 2 channels stay, so local viewers and swarmctl still work as before.
 *
 * Most of this file is given. YOUR code is in the TODO blocks:
 *   Phase 1, copy in your code:  handle_plan_req, serve_request (Task 2), on_leader_exit (Task 5)
 *   Phase 2, Task 3:             apply_command, wait_while_paused (the operator's commands)
 *   Phase 3, Task 4:             watchdog_main (heartbeats), handle_orphan (orphan rescue)
 * Your other Phase 2 code is in the transports: common/chan_shm.c (Task 1), common/chan_fifo.c
 * (Task 3) and common/chan_file.c (Task 4). Phase 3: common/chan_tcp.c, common/chan_udp.c
 * (Task 1) and center/net.c (Task 2).
 *
 * Thread map:   main     — clock loop: THEME, BEAT, waits for READY and ACK, audits, publishes
 *               readers  — one per leader incarnation: parses that leader's stdout
 *               workers  — the planner pool: pop a Request, call plan_section, reply with PLAN
 *               control  — reads operator commands from the control channel (Phase 2)
 *               signals  — turns Ctrl-C (SIGINT) and SIGTERM into a clean STOP (Phase 2)
 *               net      — Phase 3 (net.c): accepts connections; one thread per viewer, operator
 *                          and orphan; a captain's connection gets a reader thread, as above
 *               watchdog — Phase 3: declares a silent captain dead (no HEARTBEAT)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "chan.h"
#include "frame.h"
#include "kvpool.h"
#include "line_io.h"
#include "metrics.h"
#include "net.h"
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
    int   timeout_s;                      /* watchdog: abort with exit status 3 */
    int   battery_leader, battery_beats;  /* --battery L:B  -> leader L dies at beat B (first life) */
    const char *grid_mode;                /* "global" or "row" (passed to the leaders) */
    const char *name;                     /* the show's name: stage bus /<name>, control channel */
    const char *record_path;              /* --record FILE: the flight recorder (NULL = off) */
    int   start_paused;                   /* --paused: wait for RESUME before beat 0 */
    int   port;                           /* Phase 3, --port P: TCP mode (0 = any free port); -1: pipes */
    unsigned remote_mask;                 /* --remote L: captain L connects from another machine */
    int   heartbeat_ms;                   /* --heartbeat-ms: a captain silent this long is dead */
} Config;

static char default_name[32];             /* "swarm-<uid>": two students on one machine differ */

static Config cfg = {
    .score_path = "data/scores/short.score", .n_leaders = 4, .followers = 16, .tempo_override = -1,
    .think_ms = 20, .flaky_pct = 0, .pool_blocks = 256, .block_size = 1024, .workers = 2,
    .queue_cap = 8, .max_inflight = 2, .restart = 1, .timeout_s = 60, .battery_leader = -1,
    .battery_beats = -1, .grid_mode = "global", .name = default_name, .record_path = NULL,
    .start_paused = 0, .port = -1, .remote_mask = 0, .heartbeat_ms = 3000
};

/* ------------------------------------------------------------------------------- state */

typedef struct {
    int             id, inc;              /* inc: incarnation, bumped at every (re)start */
    pid_t           pid;
    chan_t         *link;                 /* the line channel to this leader (two pipes) */
    int             to_fd, from_fd;       /* the pipe ends inside `link` (for spawn_leader) */
    int             alive;                /* a process is running */
    int             gone;                 /* died and will not be restarted */
    Zone            zone;
    pthread_mutex_t wlock;                /* serializes chan_send on link */
    pthread_t       reader;
    /* Phase 3 (TCP mode) */
    int             expecting;            /* spawn_leader waits for this captain's HELLO */
    int             udp_port;             /* its radio (HELLO), for REHOME */
    char            host[64];             /* where it connected from */
    long            last_heard;           /* now_ms() of its last message (the watchdog) */
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
static long            msgs_in;                        /* lines from the captains (the load test) */
static int             cur_beat;                       /* the beat being run */

static int shutting_down;                              /* set by main under st_mu */

/* Phase 3: orphan rescue (also under st_mu) */
static long          orphan_count, rehome_count, land_count;
static unsigned char orphaned[MAX_LEADERS][MAX_FOLLOWERS];  /* 1 while its ORPHANED is handled */
static int           watchdog_stop;
static char          center_addr[64];                 /* what a local captain connects to */

/* the operator's controls (Phase 2, Task 3), also protected by st_mu */
static int paused;                                     /* PAUSE: the clock waits between beats */
static int stop_requested;                             /* STOP or Ctrl-C: end after this beat */
static int tempo_bpm;                                  /* TEMPO: read by the clock every beat */

/* the Phase 2 channels (chan.h) */
static chan_t *bus;                                    /* "shm:NAME": frames for the viewers (Task 1) */
static chan_t *rec;                                    /* "file:PATH": the flight recorder (Task 4) */
static chan_t *ctl;                                    /* "fifo:NAME": operator commands (Task 3) */

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

/* Send one message (one or more lines, without the final '\n') to leader l. */
static void leader_send_raw(int l, const char *buf, size_t len) {
    Leader *ld = &leaders[l];
    pthread_mutex_lock(&ld->wlock);
    if (ld->alive) {
        chan_send(ld->link, buf, len);            /* CHAN_CLOSED is ignored: the reader sees EOF */
    }
    pthread_mutex_unlock(&ld->wlock);
}

static void leader_send(int l, const Msg *m) {
    char line[MAX_LINE];
    int n = proto_format(m, line, sizeof(line));
    if (n < 0) {
        return;
    }
    leader_send_raw(l, line, (size_t)n);
}

static void send_busy(int l, int req_id, int retry_ms) {
    Msg m = { .type = MSG_BUSY, .req_id = req_id, .retry_ms = retry_ms };
    leader_send(l, &m);
    pthread_mutex_lock(&st_mu);
    busy_count++;
    pthread_mutex_unlock(&st_mu);
}

/* PLAN header and all keyframe lines in ONE message (one write), so nothing can interleave. */
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
    leader_send_raw(l, buf, off - 1);                /* chan_send adds the last '\n' */
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
/* The leader program: bin/leader, next to this program (found through /proc/self/exe). */
static char leader_bin[512] = "bin/leader";

static void find_leader_bin(void) {
    char self[400];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char *slash = strrchr(self, '/');
        if (slash) {
            *slash = '\0';
            snprintf(leader_bin, sizeof(leader_bin), "%s/leader", self);
        }
    }
}

static int spawn_leader_tcp(int l, int battery);

static int spawn_leader(int l, int battery) {
    if (cfg.port >= 0) {
        return spawn_leader_tcp(l, battery);             /* Phase 3 */
    }
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
    const char *bin = getenv("LEADER_BIN") ? getenv("LEADER_BIN") : leader_bin;  /* make tsan */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_IGN);                         /* Ctrl-C is for the center; it stops us */
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, NULL);           /* the center blocked SIGINT/SIGTERM */
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
        perror("exec leader");
        _exit(127);                                      /* _exit: do not flush the parent's buffers */
    }
    close(to_child[0]);
    close(from_child[1]);
    pthread_mutex_lock(&ld->wlock);                      /* wlock, then st_mu: the lock order */
    pthread_mutex_lock(&st_mu);
    ld->pid = pid;
    ld->to_fd = to_child[1];
    ld->from_fd = from_child[0];
    ld->link = chan_fd(from_child[0], to_child[1]);  /* Phase 3: a TCP connection instead */
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

/* Wait on st_cv until the absolute time deadline_ms (now_ms() clock). Call with st_mu held.
 * Returns 0 once the deadline has passed. */
static int wait_until(long deadline_ms) {
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
    pthread_cond_timedwait(&st_cv, &st_mu, &ts);
    return 1;
}

/* Phase 3 (given): start captain l as "leader --center 127.0.0.1:PORT" and wait until it has
 * connected and said HELLO (center_attach_leader below). So, as with pipes, the caller can
 * send_assign and send_theme as soon as this returns 0. A restarted captain gets --adopt: it waits
 * for its predecessor's performers (ADOPT) instead of starting new ones. A --remote captain is not
 * started the first time: we wait for it to connect from its own machine. If it dies, a local
 * replacement takes over (failover). */
static int spawn_leader_tcp(int l, int battery) {
    Leader *ld = &leaders[l];
    pthread_mutex_lock(&st_mu);
    int want = ld->inc + 1;                              /* the incarnation we are waiting for */
    pthread_mutex_unlock(&st_mu);
    int remote = want == 1 && (cfg.remote_mask >> l & 1);
    pid_t pid = 0;
    if (remote) {
        log_msg("center", "waiting for captain %d: on its machine run  bin/leader --center <this host>:%d "
                "--id %d", l, net_port(), l);
        alarm(0);                                        /* waiting for a person is not a hang */
    } else {
        char id[16], bat[16];
        snprintf(id, sizeof(id), "%d", l);
        snprintf(bat, sizeof(bat), "%d", battery);
        const char *bin = getenv("LEADER_BIN") ? getenv("LEADER_BIN") : leader_bin;
        const char *args[16];
        int k = 0;
        args[k++] = "leader";
        args[k++] = "--id";
        args[k++] = id;
        args[k++] = "--grid";
        args[k++] = cfg.grid_mode;
        args[k++] = "--center";
        args[k++] = center_addr;
        if (battery >= 0) {
            args[k++] = "--battery";
            args[k++] = bat;
        }
        if (want > 1) {
            args[k++] = "--adopt";
        }
        args[k] = NULL;
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_IGN);                     /* Ctrl-C is for the center */
            sigset_t none;
            sigemptyset(&none);
            sigprocmask(SIG_SETMASK, &none, NULL);
            execv(bin, (char *const *)args);             /* every socket of ours is close-on-exec */
            perror("exec leader");
            _exit(127);
        }
    }
    pthread_mutex_lock(&st_mu);
    ld->pid = pid;
    ld->expecting = 1;
    long deadline = now_ms() + (remote ? 300000 : 5000);
    while (ld->inc < want && !shutting_down && wait_until(deadline)) {
    }
    int ok = ld->inc == want;
    ld->expecting = 0;
    pthread_mutex_unlock(&st_mu);
    if (remote) {
        alarm((unsigned)cfg.timeout_s);
    }
    if (!ok) {
        log_msg("center", "captain %d did not connect", l);
        if (pid > 0) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
        }
        return -1;
    }
    return 0;
}

/* Phase 3 (given; net.c calls it): a connection said HELLO <l> <n> <udp_port>. If spawn_leader is
 * waiting for captain l, the connection becomes its link and gets a reader thread, exactly as the
 * pipes did. Returns 0 (the center owns `link` now) or -1 (refused: the caller closes it). */
int center_attach_leader(chan_t *link, const Msg *hello) {
    int l = hello->leader_id;
    if (l < 0 || l >= cfg.n_leaders) {
        return -1;
    }
    Leader *ld = &leaders[l];
    pthread_mutex_lock(&ld->wlock);                      /* wlock, then st_mu: the lock order */
    pthread_mutex_lock(&st_mu);
    if (!ld->expecting || ld->alive || shutting_down) {
        pthread_mutex_unlock(&st_mu);
        pthread_mutex_unlock(&ld->wlock);
        log_msg("center", "unexpected HELLO from captain %d: refused", l);
        return -1;
    }
    ld->link = link;
    ld->udp_port = hello->port;
    if (chan_addr(link, CHAN_REMOTE, ld->host, sizeof(ld->host)) < 0) {
        snprintf(ld->host, sizeof(ld->host), "127.0.0.1");
    }
    ld->last_heard = now_ms();
    ld->inc++;
    ld->alive = 1;
    ready_mask[l] = 0;
    acked[l] = cur_beat - 1;
    int *ra = malloc(2 * sizeof(int));
    ra[0] = l;
    ra[1] = ld->inc;
    int restarted = ld->inc > 1;
    pthread_t old_reader = ld->reader;
    pthread_create(&ld->reader, NULL, reader_main, ra);
    if (restarted) {
        pthread_detach(old_reader);                      /* it ends by itself (on_leader_exit) */
    }
    pthread_cond_broadcast(&st_cv);                      /* spawn_leader_tcp is waiting */
    log_msg("center", "captain %d connected from %s (radio port %d)", l, ld->host, ld->udp_port);
    pthread_mutex_unlock(&st_mu);
    pthread_mutex_unlock(&ld->wlock);
    return 0;
}

/* ------------------------------------------------- Phase 1, Task 2: the choreographer */

/* Called by a reader thread for every PLAN_REQ from leader l.
 *
 * PHASE 1 (Task 2), copy in your code: build a Request {leader, req_id, section (use
 * section_by_name), n, seed} and
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
 * PHASE 1 (Task 2): copy in your code, in this order:
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

/* ---------------------------------------------- Phase 1, Task 5: a captain went dark */

/* Reap the process, close its channel and mark it not alive. Takes wlock first, then st_mu (the
 * lock order), because senders read `alive` and `link` under wlock and the clock loop under st_mu. */
static void mark_dead(Leader *ld) {
    if (ld->pid > 0 && cfg.port < 0) {
        waitpid(ld->pid, NULL, 0);
    } else if (ld->pid > 0) {                            /* Phase 3: it may still be landing its */
        long until = now_ms() + 2000;                    /* performers: 2 s, then SIGKILL        */
        while (waitpid(ld->pid, NULL, WNOHANG) == 0) {
            if (now_ms() > until) {
                kill(ld->pid, SIGKILL);
                waitpid(ld->pid, NULL, 0);
                break;
            }
            struct timespec ts = { 0, 20000000L };
            nanosleep(&ts, NULL);
        }
    }                                                    /* pid 0: a --remote captain, not ours */
    pthread_mutex_lock(&ld->wlock);
    pthread_mutex_lock(&st_mu);
    chan_close(ld->link);                            /* closes both pipe ends */
    ld->link = NULL;
    ld->alive = 0;
    pthread_cond_broadcast(&st_cv);
    pthread_mutex_unlock(&st_mu);
    pthread_mutex_unlock(&ld->wlock);
}

/* Called when the reader of leader `l` (incarnation `inc`) reaches EOF: the process closed its
 * stdout. At the end of the show that is normal; in the middle of the show it means the captain
 * died (for example --battery).
 *
 * PHASE 1 (Task 5): copy in your code, for an UNEXPECTED death (not shutting_down):
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
    /* PHASE 1 (Task 5): copy in your code */
    (void)restart_count;
    (void)spawn_leader;
    (void)send_assign;
    (void)send_theme;
}

/* ------------------------------------------------------------------ reader threads (given) */

static void *reader_main(void *arg) {
    int l = ((int *)arg)[0], inc = ((int *)arg)[1];
    free(arg);
    pthread_mutex_lock(&st_mu);
    chan_t *link = leaders[l].link;                  /* this incarnation's channel */
    pthread_mutex_unlock(&st_mu);
    char line[MAX_LINE];
    for (;;) {
        ssize_t r = chan_recv(link, line, sizeof(line), -1);
        if (r == CHAN_CLOSED) {
            break;
        }
        __atomic_fetch_add(&msgs_in, 1, __ATOMIC_RELAXED);
        if (cfg.port >= 0) {                             /* Phase 3: any line proves it is alive */
            pthread_mutex_lock(&st_mu);
            if (inc == leaders[l].inc) {
                leaders[l].last_heard = now_ms();
            }
            pthread_mutex_unlock(&st_mu);
        }
        if (r == CHAN_TOOBIG) {
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

/* ------------------------------------------------ Phase 2: frames for the viewers (given) */

/* Fill *f with the stage as it is now, after beat b (b = -1: before the show, everyone at home).
 * The totals are the audit counters so far. */
static void build_frame(Frame *f, int b, int finished, long collisions, long speed_violations) {
    memset(f, 0, sizeof(*f));
    f->version = FRAME_VERSION;
    f->beat = b;
    f->last_beat = score_last_beat(&score);
    f->w = (uint16_t)score.stage_w;
    f->h = (uint16_t)score.stage_h;
    f->n_leaders = (uint16_t)cfg.n_leaders;
    f->n_followers = (uint16_t)cfg.followers;
    f->n_drones = (uint16_t)(cfg.n_leaders * cfg.followers);
    f->finished = (uint8_t)finished;
    int s = score_section_at(&score, b < 0 ? 0 : b);
    if (s >= 0) {
        f->section = (uint8_t)s;
        snprintf(f->section_name, sizeof(f->section_name), "%s", score.sections[s].name);
        snprintf(f->theme, sizeof(f->theme), "%s", score.sections[s].theme);
    }
    f->collisions = (uint32_t)collisions;
    f->speed_violations = (uint32_t)speed_violations;
    pthread_mutex_lock(&st_mu);
    f->tempo_bpm = (uint16_t)tempo_bpm;
    f->paused = (uint8_t)paused;
    f->busy = (uint32_t)busy_count;
    f->late = (uint32_t)late_count;
    f->restarts = (uint32_t)restart_count;
    for (int l = 0; l < cfg.n_leaders; l++) {
        if (leaders[l].alive) {
            f->alive_mask |= 1u << l;
        }
        for (int k = 0; k < cfg.followers; k++) {
            DroneView *d = &f->drones[l * cfg.followers + k];
            d->x = (uint8_t)spos[l][k].x;
            d->y = (uint8_t)spos[l][k].y;
            d->color = scol[l][k];
            d->leader = (uint8_t)l;
            d->state = leaders[l].alive ? DRONE_OK : orphaned[l][k] ? DRONE_ORPHAN : DRONE_LOST;
        }
    }
    pthread_mutex_unlock(&st_mu);
    frame_seal(f);
}

/* Publish a frame to every viewer (the stage bus; Phase 3: also every TCP viewer) and, if
 * `record`, to the flight recorder: the same chan_send for both. Only the main thread calls this:
 * ONE sender per channel. */
static void publish(const Frame *f, int record) {
    if (bus) {
        chan_send(bus, f, sizeof(*f));
    }
    if (cfg.port >= 0) {
        net_publish(f);                                  /* Phase 3 (net.c): must never block */
    }
    if (record && rec) {
        chan_send(rec, f, sizeof(*f));
    }
}

/* --------------------------------------------- Phase 2, Task 3: the operator's commands */

/* End the show cleanly after the current beat (STOP, Ctrl-C). Given. */
static void request_stop(void) {
    pthread_mutex_lock(&st_mu);
    stop_requested = 1;
    pthread_cond_broadcast(&st_cv);
    pthread_mutex_unlock(&st_mu);
}

/* Apply one command line from the control channel:
 *   PAUSE                stop the beat clock after the current beat
 *   RESUME               start it again
 *   TEMPO <bpm>          change the tempo; 0 = as fast as possible (0..2000)
 *   KILL <leader_id>     kill that captain with SIGKILL (your Phase 1 restart must handle it)
 *   STOP                 end the show early, cleanly (same as Ctrl-C on the center)
 *
 * TODO (Phase 2, Task 3):
 *   PAUSE, RESUME   set or clear `paused` under st_mu and broadcast st_cv (the clock waits on it)
 *   TEMPO <bpm>     0..2000 into `tempo_bpm` under st_mu; the clock loop reads it every beat
 *   KILL <id>       0 <= id < cfg.n_leaders: if that captain is alive, kill(pid, SIGKILL). Read
 *                   `alive` and `pid` under st_mu. Your Phase 1 on_leader_exit does the rest.
 *   STOP            request_stop()
 * Anything else (unknown word, missing, extra or non-numeric argument, out of range): log it with
 * log_msg and ignore it; never crash. Parse with strtok_r or sscanf, not strtok (why?).
 * Returns 0 if the command was applied, -1 if it was rejected. */
static int apply_command(const char *line) {
    (void)line;
    return -1;
}

/* The clock loop calls this between two beats while `paused` is set, with st_mu held.
 *
 * TODO (Phase 2, Task 3): wait on st_cv until the show is resumed or stopped. No busy-waiting, no sleep. */
static void wait_while_paused(void) {
}

/* The control thread (given): one command at a time. It waits at most 200 ms per chan_recv, so
 * it notices shutting_down soon after main sets it. */
static void *control_main(void *arg) {
    (void)arg;
    char line[MAX_LINE];
    for (;;) {
        pthread_mutex_lock(&st_mu);
        int closing = shutting_down;
        pthread_mutex_unlock(&st_mu);
        if (closing) {
            break;
        }
        ssize_t r = chan_recv(ctl, line, sizeof(line), 200);
        if (r >= 0) {
            apply_command(line);
        } else if (r == CHAN_TOOBIG) {
            log_msg("center", "control: command too long, ignored");
        } else if (r == CHAN_CLOSED) {
            log_msg("center", "control channel closed");
            break;
        }
    }
    return NULL;
}

/* --------------------------------------- Phase 3: what net.c may call (given wrappers) */

/* An operator's command line (OPERATE): the same apply_command as the control channel. */
int center_command(const char *line) {
    return apply_command(line);
}

static int handle_orphan(chan_t *c, const Msg *m);

/* An orphaned performer's ORPHANED, on its own connection c. Counts it, marks the drone as an
 * orphan in the frames while it waits, and lets handle_orphan answer. */
void center_orphan(chan_t *c, const Msg *m) {
    int l = m->leader_id, f = m->follower;
    if (l < 0 || l >= cfg.n_leaders || f < 0 || f >= cfg.followers) {
        log_msg("center", "ORPHANED from an unknown performer %d of captain %d", f, l);
        return;
    }
    pthread_mutex_lock(&st_mu);
    orphan_count++;
    orphaned[l][f] = 1;
    pthread_mutex_unlock(&st_mu);
    log_msg("center", "performer %d of captain %d is an orphan (last beat %d)", f, l, m->beat);
    int r = handle_orphan(c, m);
    pthread_mutex_lock(&st_mu);
    orphaned[l][f] = 0;
    if (r == 1) {
        rehome_count++;
    } else if (r == 0) {
        land_count++;
    }
    pthread_mutex_unlock(&st_mu);
}

/* ------------------------------------------------- Phase 3, Task 4: watchdog and orphans */

#define ORPHAN_WAIT_MS 5000           /* how long an orphan may wait for its zone's next captain */

static int captain_settled(int l) {   /* for wait_for: alive with a radio, gone, or the end */
    return shutting_down || leaders[l].gone || (leaders[l].alive && leaders[l].udp_port > 0);
}

/* An orphaned performer asked for a new captain: ORPHANED <f> <l> <last_beat>, on its own TCP
 * connection c (net.c calls center_orphan, which calls this; f and l are already checked).
 *
 * TODO (Phase 3, Task 4): find the orphan a captain, or land it.
 *   1. Wait (wait_for(captain_settled, l, now_ms() + ORPHAN_WAIT_MS), with st_mu held) until zone
 *      l has a live captain with a radio. It may still be the old one (a false alarm: a lossy
 *      radio; or not yet declared dead), a restarted one (your on_leader_exit), or none: gone for
 *      good (--no-restart) or the show is shutting down.
 *   2. A live captain: send it ADOPT <f> (leader_send) FIRST, so that it expects the performer,
 *      then answer the orphan on c with REHOME <f> <host> <udp_port>: leaders[l].host and
 *      .udp_port, read under st_mu. If that captain runs on this machine (host 127.0.0.1 or ::1),
 *      the orphan may not (a --remote captain's performers): answer with the address the orphan
 *      used to reach us instead, chan_addr(c, CHAN_LOCAL, ...). Return 1.
 *   3. Otherwise answer LAND <f> and return 0. Return -1 if nothing could be sent.
 * Use proto_format for the answers. Never hold st_mu while you send (why not?). */
static int handle_orphan(chan_t *c, const Msg *m) {
    (void)c;
    (void)m;
    (void)captain_settled;
    return -1;                                       /* no answer: the orphan retries, then lands */
}

/* The heartbeat watchdog thread (TCP mode only). A captain sends HEARTBEAT every 500 ms, and the
 * reader updates leaders[l].last_heard on EVERY line it gets (given).
 *
 * TODO (Phase 3, Task 4): about every 100 ms, until watchdog_stop is set (under st_mu): for each
 * captain that is alive, if now_ms() - last_heard > cfg.heartbeat_ms, declare it dead:
 *   - log "captain <l> silent for <ms> ms: declared dead"
 *   - kill(pid, SIGKILL) if pid > 0 (a --remote captain has pid 0: not ours to kill)
 *   - chan_shutdown(link): its reader gets CHAN_CLOSED and your on_leader_exit does the rest
 *     (restart; its performers become orphans and ask handle_orphan for a new captain)
 * Use link only while you hold wlock and then st_mu (the lock order): mark_dead closes it.
 * Test: SIGSTOP a captain (kill -STOP <pid>): it does not die, it just stops talking. */
static void *watchdog_main(void *arg) {
    (void)arg;
    return NULL;
}

/* --------------------------------------------- Ctrl-C and SIGTERM: a clean stop (given) */

/* main blocks SIGINT and SIGTERM before it creates any thread, so every thread inherits the
 * blocked mask and this one thread receives them with sigwait(): no signal handler, so it may
 * call any function. (Signals are Week 14; Phase 3 builds on this.) */
static sigset_t stop_signals;

static void *signal_main(void *arg) {
    (void)arg;
    for (;;) {
        int sig = 0;
        if (sigwait(&stop_signals, &sig) != 0) {
            continue;
        }
        pthread_mutex_lock(&st_mu);
        int closing = shutting_down;
        pthread_mutex_unlock(&st_mu);
        if (closing) {
            break;                                   /* main woke us up to finish */
        }
        log_msg("center", "signal %d: stopping the show", sig);
        request_stop();
    }
    return NULL;
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
        else if (!strcmp(a, "--name") && has)         cfg.name = argv[++i];
        else if (!strcmp(a, "--record") && has)       cfg.record_path = argv[++i];
        else if (!strcmp(a, "--paused"))              cfg.start_paused = 1;
        else if (!strcmp(a, "--timeout") && has)      cfg.timeout_s = atoi(argv[++i]);
        else if (!strcmp(a, "--grid") && has)         cfg.grid_mode = argv[++i];
        else if (!strcmp(a, "--port") && has)         cfg.port = atoi(argv[++i]);          /* Phase 3 */
        else if (!strcmp(a, "--heartbeat-ms") && has) cfg.heartbeat_ms = atoi(argv[++i]);
        else if (!strcmp(a, "--remote") && has) {
            int r = atoi(argv[++i]);
            if (r < 0 || r >= MAX_LEADERS) {
                return -1;
            }
            cfg.remote_mask |= 1u << r;
        }
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
        (strcmp(cfg.grid_mode, "global") && strcmp(cfg.grid_mode, "row")) ||
        cfg.name[0] == '\0' || strlen(cfg.name) > 24 || strchr(cfg.name, '/') ||
        cfg.port < -1 || cfg.port > 65535 || cfg.heartbeat_ms < 100 ||
        (cfg.remote_mask && cfg.port < 0) || (cfg.remote_mask >> cfg.n_leaders)) {
        fprintf(stderr, "center: argument out of range\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    snprintf(default_name, sizeof(default_name), "swarm-%u", (unsigned)getuid());
    find_leader_bin();
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr,
                "usage: center [--score F] [--leaders N] [--followers K] [--tempo BPM|0] [--think-ms MS]\n"
                "              [--flaky PCT] [--pool-blocks N] [--workers N] [--queue-cap N]\n"
                "              [--max-inflight N] [--grid global|row] [--battery L:B] [--no-restart]\n"
                "              [--timeout SEC] [--name NAME] [--record FILE] [--paused]\n"
                "              [--port P [--remote L]... [--heartbeat-ms MS]]      (Phase 3: TCP)\n");
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, on_alarm);
    alarm((unsigned)cfg.timeout_s);

    /* Phase 2: SIGINT/SIGTERM go to signal_main only (block them before any thread exists). */
    sigemptyset(&stop_signals);
    sigaddset(&stop_signals, SIGINT);
    sigaddset(&stop_signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &stop_signals, NULL);

    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&st_cv, &ca);
    for (int l = 0; l < MAX_LEADERS; l++) {
        pthread_mutex_init(&leaders[l].wlock, NULL);     /* before any thread can use one */
    }

    if (score_load(cfg.score_path, &score) != 0) {
        return 1;
    }
    if (score.image_path[0] && image_load(score.image_path, &target) != 0) {
        return 1;
    }
    tempo_bpm = cfg.tempo_override >= 0 ? cfg.tempo_override : score.tempo_bpm;
    paused = cfg.start_paused;
    int last_beat = score_last_beat(&score);
    int ntot = cfg.n_leaders * cfg.followers;

    /* Phase 2: the stage bus, the flight recorder, the control channel. The show also runs
     * without them (so your Phase 1 code can be tested first), but then nobody can watch it. */
    char addr[256];
    snprintf(addr, sizeof(addr), "shm:%s", cfg.name);
    bus = chan_open(addr, CHAN_SEND);
    if (!bus) {
        log_msg("center", "no stage bus %s (Task 1 not done?): viewers cannot watch", addr);
    }
    if (cfg.record_path) {
        snprintf(addr, sizeof(addr), "file:%s", cfg.record_path);
        rec = chan_open(addr, CHAN_SEND);
        if (!rec) {
            log_msg("center", "no flight recorder %s (Task 4 not done?)", addr);
        }
    }
    ctl_addr(cfg.name, addr, sizeof(addr));
    ctl = chan_open(addr, CHAN_RECV);
    if (!ctl) {
        log_msg("center", "no control channel %s (Task 3 not done?)", addr);
    }
    pthread_t sig_thread, ctl_thread;
    pthread_create(&sig_thread, NULL, signal_main, NULL);
    if (ctl) {
        pthread_create(&ctl_thread, NULL, control_main, NULL);
    }

    PlannerOpts po = { cfg.think_ms, cfg.flaky_pct };
    planner_init(&po);
    pool = kv_pool_create((size_t)cfg.block_size, (size_t)cfg.pool_blocks);
    req_queue = queue_create((size_t)cfg.queue_cap, sizeof(Request));
    sem_init(&inflight, 0, (unsigned)cfg.max_inflight);

    /* Phase 3: the TCP server. The captains connect back to it, so it starts before them. */
    pthread_t wd_thread;
    int net_ok = 1;
    if (cfg.port >= 0 && pool && req_queue) {
        prctl(PR_SET_CHILD_SUBREAPER, 1);                /* orphaned performers become OUR children */
        snprintf(addr, sizeof(addr), "tcp::%d", cfg.port);
        net_ok = net_start(addr) == 0;
        if (net_ok) {
            int p = net_port();
            snprintf(center_addr, sizeof(center_addr), "127.0.0.1:%d", p);
            log_msg("center", "listening on tcp port %d", p);
            pthread_create(&wd_thread, NULL, watchdog_main, NULL);
        } else {
            log_msg("center", "cannot serve %s (Phase 3, Tasks 1 and 2 not done?)", addr);
        }
    }
    if (!pool || !req_queue || !net_ok) {
        if (net_ok) {
            fprintf(stderr, "center: pool or queue missing (implement queue.c; check the pool)\n");
        }
        pthread_mutex_lock(&st_mu);
        shutting_down = 1;
        pthread_mutex_unlock(&st_mu);
        if (ctl) {
            pthread_join(ctl_thread, NULL);
            chan_close(ctl);
        }
        pthread_kill(sig_thread, SIGTERM);
        pthread_join(sig_thread, NULL);
        chan_close(rec);
        chan_close(bus);
        if (req_queue) {
            queue_destroy(req_queue);
        }
        if (pool) {
            kv_pool_destroy(pool);
        }
        image_free(&target);
        return 1;
    }
    pthread_t workers[16];
    int nworkers = cfg.workers > 16 ? 16 : cfg.workers;
    for (int i = 0; i < nworkers; i++) {
        pthread_create(&workers[i], NULL, worker_main, NULL);
    }

    cur_beat = 0;
    for (int l = 0; l < cfg.n_leaders; l++) {
        leaders[l].id = l;
        zone_split(score.stage_w, score.stage_h, cfg.n_leaders, l, &leaders[l].zone);
        for (int f = 0; f < cfg.followers; f++) {
            home_position(&leaders[l].zone, f, &spos[l][f].x, &spos[l][f].y);
            prevpos[l][f] = spos[l][f];
            scol[l][f] = 15;
        }
        if (spawn_leader(l, l == cfg.battery_leader ? cfg.battery_beats : -1) != 0) {
            return 1;                                    /* exit() takes the others down with it */
        }
        send_assign(l);
        send_theme(l, 0, score.sections[0].start_beat);
    }

    Frame frame;                                          /* 4 KB: one per beat, reused */
    build_frame(&frame, -1, 0, 0, 0);                     /* "beat -1": everyone at home */
    publish(&frame, 1);

    long t_start = now_ms();
    long t_beat = t_start;
    long collisions = 0, speed_violations = 0;
    int beats_run = 0, last_period = -1;
    for (int b = 0; b <= last_beat; b++) {
        int s = score_section_at(&score, b);

        /* Phase 2: the operator's controls, between two beats */
        pthread_mutex_lock(&st_mu);
        if (paused && !stop_requested) {
            pthread_mutex_unlock(&st_mu);
            build_frame(&frame, b - 1, 0, collisions, speed_violations);
            publish(&frame, 0);                           /* the viewers show "paused" */
            pthread_mutex_lock(&st_mu);
            alarm(0);                                     /* a pause is not a hang */
            wait_while_paused();                          /* Task 3 */
            t_beat = now_ms();
        }
        int stop = stop_requested;
        int period_ms = tempo_bpm > 0 ? 60000 / tempo_bpm : 0;
        pthread_mutex_unlock(&st_mu);
        if (stop) {
            log_msg("center", "stopped by the operator after beat %d", b - 1);
            break;
        }
        if (period_ms != last_period) {                   /* a new tempo starts now */
            t_beat = now_ms();
            last_period = period_ms;
        }
        alarm((unsigned)cfg.timeout_s);                   /* the watchdog: per beat */
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
        beats_run = b + 1;

        build_frame(&frame, b, 0, collisions, speed_violations);     /* Phase 2: publish */
        publish(&frame, 1);

        if (period_ms) {
            t_beat += period_ms;
            long left = t_beat - now_ms();
            if (left > 0) {
                struct timespec ts = { left / 1000, (left % 1000) * 1000000L };
                nanosleep(&ts, NULL);
            }
        }
    }
    alarm((unsigned)cfg.timeout_s);                       /* the shutdown must not hang either */

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
    build_frame(&frame, beats_run - 1, 1, collisions, speed_violations);   /* the final frame */
    publish(&frame, 1);

    /* shutdown */
    pthread_mutex_lock(&st_mu);
    shutting_down = 1;
    pthread_cond_broadcast(&st_cv);                       /* Phase 3: waiting orphans get LAND */
    pthread_mutex_unlock(&st_mu);
    Msg sm = { .type = MSG_SHUTDOWN };
    for (int l = 0; l < cfg.n_leaders; l++) {
        leader_send(l, &sm);
    }
    for (int l = 0; l < cfg.n_leaders; l++) {
        pthread_join(leaders[l].reader, NULL);
    }
    if (cfg.port >= 0) {                                  /* the watchdog ran until the captains left */
        pthread_mutex_lock(&st_mu);
        watchdog_stop = 1;
        pthread_mutex_unlock(&st_mu);
        pthread_join(wd_thread, NULL);
    }
    queue_close(req_queue);
    for (int i = 0; i < nworkers; i++) {
        pthread_join(workers[i], NULL);
    }
    if (cfg.port >= 0) {
        net_stop();                                       /* viewers get the final frame first */
        long until = now_ms() + 1500;                     /* adopted performers are our children */
        for (;;) {
            pid_t p = waitpid(-1, NULL, WNOHANG);
            if (p < 0) {
                break;                                    /* ECHILD: nobody left */
            }
            if (p == 0 && now_ms() > until) {
                log_msg("center", "some performers did not land");
                break;
            }
            if (p == 0) {
                struct timespec ts = { 0, 20000000L };
                nanosleep(&ts, NULL);
            }
        }
    }
    if (ctl) {
        pthread_join(ctl_thread, NULL);                   /* it sees shutting_down within 200 ms */
        chan_close(ctl);                                  /* removes the FIFO */
    }
    pthread_kill(sig_thread, SIGTERM);                    /* sigwait returns, sees shutting_down */
    pthread_join(sig_thread, NULL);
    chan_close(rec);                                      /* the recording is complete */
    chan_close(bus);                                      /* the viewers see the end of the show */

    double elapsed = (double)(now_ms() - t_start) / 1000.0;
    printf("RESULT beats=%d drones=%d collisions=%ld speed_violations=%ld mosaic_accuracy=%.3f "
           "busy=%ld late=%ld restarts=%ld elapsed=%.2f orphans=%ld rehomed=%ld landed=%ld msgs=%ld\n",
           beats_run, ntot, collisions, speed_violations, acc, busy_count, late_count,
           restart_count, elapsed, orphan_count, rehome_count, land_count, msgs_in);
    queue_destroy(req_queue);
    kv_pool_destroy(pool);
    image_free(&target);
    return (collisions == 0 && speed_violations == 0) ? 0 : 2;
}
