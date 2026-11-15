/* test_chan.c — checks of YOUR transports, without the center (given).
 *
 *   tests/test_chan shm      the stage bus (Task 1)
 *   tests/test_chan fifo     the control channel (Task 3; set CTL_TRANSPORT to test "mq" instead)
 *   tests/test_chan file     the flight recorder (Task 4)
 *   tests/test_chan all      the three above (Phase 2)
 *   tests/test_chan tcp      TCP: connect, listen, accept (Phase 3, Task 1)
 *   tests/test_chan udp      UDP: the radio (Phase 3, Task 1)
 *   tests/test_chan net      tcp and udp
 *
 * Every check prints PASS or FAIL; the exit status is the number of failures. A check that hangs
 * is reported as a FAIL after a timeout (a send that waits for a receiver? a lost wake-up?).
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "chan.h"
#include "frame.h"
#include "recording.h"
#include "swarm.h"

static int fails;
static const char *current = "";

static void check(int ok, const char *what, const char *detail) {
    if (ok) {
        printf("PASS  %s\n", what);
    } else {
        printf("FAIL  %s\n", what);
        if (detail && *detail) {
            printf("      %s\n", detail);
        }
        fails++;
    }
    fflush(stdout);
}

static void on_alarm(int sig) {
    (void)sig;
    char msg[256];
    int n = snprintf(msg, sizeof(msg), "FAIL  %s\n      HANG: still blocked after the time limit "
                     "(a send that waits for a receiver? a lost wake-up?)\n", current);
    if (write(1, msg, (size_t)n) < 0) {
        _exit(99);
    }
    _exit(99);
}

static void step(const char *name, unsigned seconds) {
    current = name;
    alarm(seconds);
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static long cpu_ms(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000L +
           (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000L;
}

/* A frame whose every byte depends on its beat: a torn copy fails frame_intact(). */
static void make_frame(Frame *f, int beat) {
    memset(f, 0, sizeof(*f));
    f->version = FRAME_VERSION;
    f->beat = beat;
    f->last_beat = 1 << 30;
    f->w = 64;
    f->h = 36;
    f->n_leaders = 4;
    f->n_followers = 128;
    f->n_drones = FRAME_MAX_DRONES;
    snprintf(f->section_name, sizeof(f->section_name), "s%d", beat % 1000);
    for (int i = 0; i < FRAME_MAX_DRONES; i++) {
        f->drones[i].x = (uint8_t)((i * 7 + beat) % 64);
        f->drones[i].y = (uint8_t)((i / 64 + beat) % 36);
        f->drones[i].color = (uint8_t)((i + beat) % 16);
        f->drones[i].leader = (uint8_t)(i / 128);
        f->drones[i].pad[0] = (uint8_t)beat;
    }
    frame_seal(f);
}

/* ================================================================================ shm */

/* the largest mapping of /dev/shm/NAME in this process must not be writable */
static int largest_mapping_read_only(const char *name) {
    char needle[128], line[512];
    snprintf(needle, sizeof(needle), "/dev/shm/%s", name);
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return -1;
    }
    unsigned long best = 0;
    char best_perm[8] = "";
    while (fgets(line, sizeof(line), fp)) {
        unsigned long a, b;
        char perm[8];
        if (strstr(line, needle) && sscanf(line, "%lx-%lx %7s", &a, &b, perm) == 3 && b - a > best) {
            best = b - a;
            snprintf(best_perm, sizeof(best_perm), "%s", perm);
        }
    }
    fclose(fp);
    if (best == 0) {
        return -1;
    }
    return best_perm[1] != 'w';
}

/* a receiver process for the stress test: exit 0 ok, 1 torn, 2 order, 3 no final, 4 open, 5 silent */
static int receiver_child(const char *addr, int ready_fd, int last_beat) {
    chan_t *r = chan_open(addr, CHAN_RECV);
    char c = r ? 'y' : 'n';
    if (write(ready_fd, &c, 1) != 1 || !r) {
        return 4;
    }
    close(ready_fd);
    static Frame f;
    int prev = -1, torn = 0, order = 0, got_final = 0, silent = 0;
    for (;;) {
        ssize_t n = chan_recv(r, &f, sizeof(f), 3000);
        if (n == CHAN_CLOSED) {
            break;
        }
        if (n == CHAN_TIMEOUT) {
            silent = 1;
            break;
        }
        if (n != (ssize_t)sizeof(f) || !frame_intact(&f)) {
            torn++;
            continue;
        }
        if (f.beat <= prev) {
            order++;
        }
        prev = f.beat;
        got_final |= f.beat == last_beat;
    }
    chan_close(r);
    return torn ? 1 : order ? 2 : silent ? 5 : got_final ? 0 : 3;
}

static void test_shm(void) {
    char name[64], addr[80], name2[64], addr2[80], buf[256];
    snprintf(name, sizeof(name), "tc-%d", (int)getpid());
    snprintf(addr, sizeof(addr), "shm:%s", name);
    snprintf(name2, sizeof(name2), "tc2-%d", (int)getpid());
    snprintf(addr2, sizeof(addr2), "shm:%s", name2);
    static Frame f, g;

    step("shm: the sender creates the bus", 5);
    chan_t *s = chan_open(addr, CHAN_SEND);
    snprintf(buf, sizeof(buf), "/dev/shm/%s", name);
    check(s && access(buf, F_OK) == 0, "shm: the sender creates the bus in /dev/shm",
          s ? "no object in /dev/shm" : "chan_open(shm:..., CHAN_SEND) returned NULL");
    if (!s) {
        return;
    }
    step("shm: a second sender with the same name", 5);
    chan_t *s2 = chan_open(addr, CHAN_SEND);
    check(!s2, "shm: a second sender with the same name is refused while the first is alive", "");
    chan_close(s2);

    step("shm: a receiver attaches", 5);
    chan_t *r = chan_open(addr, CHAN_RECV);
    check(r != NULL, "shm: a receiver attaches", "chan_open(shm:..., CHAN_RECV) returned NULL");
    if (!r) {
        chan_close(s);
        return;
    }
    check(largest_mapping_read_only(name) == 1, "shm: the receiver maps the frames read-only",
          "in /proc/self/maps the largest mapping of the bus is writable (use PROT_READ)");

    step("shm: send and receive one frame", 5);
    make_frame(&f, 1);
    int rs = chan_send(s, &f, sizeof(f));
    ssize_t n = chan_recv(r, &g, sizeof(g), 1000);
    check(rs == 0 && n == (ssize_t)sizeof(g) && frame_intact(&g) && g.beat == 1,
          "shm: one frame arrives whole", "chan_send/chan_recv did not deliver the frame");

    step("shm: newest value wins", 5);
    for (int b = 2; b <= 4; b++) {
        make_frame(&f, b);
        chan_send(s, &f, sizeof(f));
    }
    n = chan_recv(r, &g, sizeof(g), 1000);
    check(n == (ssize_t)sizeof(g) && g.beat == 4, "shm: a receiver gets the newest frame (4 of 2,3,4)",
          n == (ssize_t)sizeof(g) ? "got an older frame" : "no frame");
    chan_t *late = chan_open(addr, CHAN_RECV);
    n = late ? chan_recv(late, &g, sizeof(g), 500) : CHAN_CLOSED;
    check(n == (ssize_t)sizeof(g) && g.beat == 4, "shm: a receiver that attaches late gets the current frame at once",
          late ? "no frame, or not the newest one" : "chan_open failed");
    chan_close(late);
    long t0 = now_ms(), c0 = cpu_ms();
    n = chan_recv(r, &g, sizeof(g), 300);
    long dt = now_ms() - t0, dc = cpu_ms() - c0;
    check(n == CHAN_TIMEOUT && dt >= 250 && dt < 1500,
          "shm: nothing new -> CHAN_TIMEOUT after about timeout_ms", "wrong result or wrong wait time");
    check(dc < 60, "shm: waiting does not burn the CPU (no busy-waiting)",
          "the receiver used a lot of CPU time while waiting 300 ms");

    step("shm: too big", 5);
    static char big[sizeof(Frame) + 16];
    check(chan_send(s, big, sizeof(Frame) + 1) == CHAN_TOOBIG, "shm: a message bigger than a Frame is refused",
          "expected CHAN_TOOBIG");
    make_frame(&f, 5);
    chan_send(s, &f, sizeof(f));
    n = chan_recv(r, buf, 100, 1000);
    check(n == CHAN_TOOBIG, "shm: a buffer smaller than the message gives CHAN_TOOBIG", "");
    chan_close(r);

    /* ---- many receivers: normal, slow (stopped) and killed; the sender must never wait ---- */
    step("shm: stress with stopped and killed receivers", 60);
    const int N = 20000, K = 3, B0 = 100;         /* beats above the ones sent so far: a new
                                                       receiver first gets the current frame (5) */
    pid_t kids[K + 2];
    for (int k = 0; k < K + 2; k++) {
        int p[2];
        if (pipe(p) != 0) {
            return;
        }
        kids[k] = fork();
        if (kids[k] == 0) {
            close(p[0]);
            alarm(60);
            _exit(receiver_child(addr, p[1], B0 + N - 1));
        }
        close(p[1]);
        char c = 0;
        if (read(p[0], &c, 1) != 1) {
            c = 'n';
        }
        close(p[0]);
    }
    kill(kids[K], SIGSTOP);                          /* a receiver that never runs again */
    long t1 = now_ms();
    for (int b = B0; b < B0 + N; b++) {
        make_frame(&f, b);
        chan_send(s, &f, sizeof(f));
        if (b == B0 + N / 3) {
            kill(kids[K + 1], SIGKILL);              /* a receiver dies in the middle */
        }
    }
    long send_ms = now_ms() - t1;
    check(1, "shm: 20000 frames sent with a stopped and a killed receiver (the sender never waited)", "");
    printf("      (%ld ms, %.1f us per frame)\n", send_ms, send_ms * 1000.0 / N);
    chan_close(s);
    snprintf(buf, sizeof(buf), "/dev/shm/%s", name);
    check(access(buf, F_OK) != 0, "shm: the sender's chan_close removes the bus from /dev/shm", buf);
    kill(kids[K], SIGCONT);
    int bad_torn = 0, bad_order = 0, bad_final = 0, bad_other = 0;
    for (int k = 0; k < K + 2; k++) {
        int st = 0;
        waitpid(kids[k], &st, 0);
        if (k == K + 1) {
            continue;                                /* the killed one */
        }
        int code = WIFEXITED(st) ? WEXITSTATUS(st) : 100;
        bad_torn += code == 1;
        bad_order += code == 2;
        bad_final += code == 3 || code == 5;
        bad_other += code == 4 || code >= 6;
    }
    check(bad_torn == 0, "shm: no receiver ever got a torn frame", "a receiver copied a frame while it was being written");
    check(bad_order == 0, "shm: frames never go back in time", "a receiver got an older frame after a newer one");
    check(bad_final == 0, "shm: every receiver (also the stopped one) gets the final frame, then CHAN_CLOSED",
          "a receiver missed the last frame or did not see the close");
    check(bad_other == 0, "shm: receivers attach and exit cleanly", "a receiver failed to attach or crashed");

    /* ---- the sender dies without closing ---- */
    step("shm: a crashed sender", 15);
    int p[2];
    if (pipe(p) != 0) {
        return;
    }
    pid_t crash = fork();
    if (crash == 0) {
        close(p[0]);
        chan_t *cs = chan_open(addr2, CHAN_SEND);
        make_frame(&f, 7);
        if (cs) {
            chan_send(cs, &f, sizeof(f));
        }
        char c = cs ? 'y' : 'n';
        if (write(p[1], &c, 1) != 1) {
            _exit(1);
        }
        pause();                                     /* until SIGKILL: no chan_close */
        _exit(0);
    }
    close(p[1]);
    char c = 'n';
    if (read(p[0], &c, 1) != 1) {
        c = 'n';
    }
    close(p[0]);
    chan_t *r2 = c == 'y' ? chan_open(addr2, CHAN_RECV) : NULL;
    n = r2 ? chan_recv(r2, &g, sizeof(g), 1000) : -9;
    kill(crash, SIGKILL);
    waitpid(crash, NULL, 0);
    t0 = now_ms();
    ssize_t n2 = r2 ? chan_recv(r2, &g, sizeof(g), 5000) : -9;
    dt = now_ms() - t0;
    check(r2 && n == (ssize_t)sizeof(g) && n2 == CHAN_CLOSED && dt < 3000,
          "shm: a receiver notices a sender that died without closing (CHAN_CLOSED, < 3 s)",
          r2 ? "chan_recv kept waiting for a dead sender" : "could not set up the crashed sender");
    chan_close(r2);
    chan_t *s3 = chan_open(addr2, CHAN_SEND);
    check(s3 != NULL, "shm: a new sender can reuse the name of a crashed one", "chan_open returned NULL");
    chan_close(s3);
    alarm(0);
}

/* ================================================================================ fifo */

/* all of this process's descriptors that point to `path` have FD_CLOEXEC */
static int fds_cloexec(const char *path) {
    DIR *d = opendir("/proc/self/fd");
    if (!d) {
        return -1;
    }
    int found = 0, ok = 1;
    struct dirent *e;
    while ((e = readdir(d))) {
        char link[320], target[256];
        snprintf(link, sizeof(link), "/proc/self/fd/%s", e->d_name);
        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if (n <= 0) {
            continue;
        }
        target[n] = '\0';
        if (atoi(e->d_name) > 2 && strstr(target, path)) {      /* 0-2 are inherited */
            found++;
            int fl = fcntl(atoi(e->d_name), F_GETFD);
            if (fl < 0 || !(fl & FD_CLOEXEC)) {
                ok = 0;
            }
        }
    }
    closedir(d);
    return found ? ok : -1;
}

static void test_fifo(void) {
    char name[64], addr[80], path[128], line[MAX_LINE + 64];
    snprintf(name, sizeof(name), "tc-%d", (int)getpid());
    snprintf(addr, sizeof(addr), "%s:%s", CTL_TRANSPORT, name);
    snprintf(path, sizeof(path), "/tmp/%s.ctl", name);
    int is_fifo = !strcmp(CTL_TRANSPORT, "fifo");

    step("ctl: no receiver", 5);
    long t0 = now_ms();
    chan_t *nobody = chan_open(addr, CHAN_SEND);
    check(!nobody && now_ms() - t0 < 500, "ctl: a sender with no receiver fails at once", "");
    chan_close(nobody);

    step("ctl: the receiver creates the channel", 5);
    chan_t *r = chan_open(addr, CHAN_RECV);
    struct stat st;
    check(r && (!is_fifo || (stat(path, &st) == 0 && S_ISFIFO(st.st_mode))),
          "ctl: the receiver creates the channel", is_fifo ? path : "chan_open returned NULL");
    if (!r) {
        return;
    }
    if (is_fifo) {
        check(fds_cloexec(path) == 1, "ctl: the receiver's descriptors are close-on-exec (O_CLOEXEC)",
              "a leader forked by the center would inherit them");
    }

    step("ctl: one command", 5);
    chan_t *s = chan_open(addr, CHAN_SEND);
    int rs = s ? chan_send(s, "PAUSE", 5) : -9;
    chan_close(s);
    ssize_t n = chan_recv(r, line, sizeof(line), 1000);
    check(rs == 0 && n == 5 && !strcmp(line, "PAUSE"), "ctl: a command arrives as one message",
          "expected the 5 bytes PAUSE");

    step("ctl: a sender that leaves", 5);
    t0 = now_ms();
    n = chan_recv(r, line, sizeof(line), 300);
    long dt = now_ms() - t0;
    check(n == CHAN_TIMEOUT && dt >= 250 && dt < 1500,
          "ctl: after a sender leaves, the receiver just waits (CHAN_TIMEOUT, not CHAN_CLOSED)",
          n == CHAN_CLOSED ? "got CHAN_CLOSED: the last writer closing is not the end of the channel"
                           : "wrong result or wrong wait time");
    s = chan_open(addr, CHAN_SEND);
    if (s) {
        chan_send(s, "TEMPO 90", 8);
        chan_close(s);
    }
    n = chan_recv(r, line, sizeof(line), 1000);
    check(n == 8 && !strcmp(line, "TEMPO 90"), "ctl: a second sender is heard too", "");

    step("ctl: too long", 5);
    s = chan_open(addr, CHAN_SEND);
    char longline[400];
    memset(longline, 'A', sizeof(longline));
    int tl = s ? chan_send(s, longline, sizeof(longline)) : -9;
    chan_close(s);
    check(tl == CHAN_TOOBIG, "ctl: a command longer than MAX_LINE is refused by chan_send", "");

    step("ctl: many senders at once", 20);
    const int W = 4, L = 200;
    pid_t kids[4];
    for (int k = 0; k < W; k++) {
        kids[k] = fork();
        if (kids[k] == 0) {
            chan_t *ws = chan_open(addr, CHAN_SEND);
            for (int i = 0; ws && i < L; i++) {
                char m[64];
                int len = snprintf(m, sizeof(m), "W%d %d xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", k, i);
                chan_send(ws, m, (size_t)len);
            }
            chan_close(ws);
            _exit(ws ? 0 : 1);
        }
    }
    int next[4] = {0, 0, 0, 0}, bad = 0, got = 0;
    while (got < W * L) {
        n = chan_recv(r, line, sizeof(line), 2000);
        if (n < 0) {
            break;
        }
        int k, i;
        char tail[64];
        if (sscanf(line, "W%d %d %63s", &k, &i, tail) != 3 || k < 0 || k >= W || i != next[k] ||
            strlen(tail) != 40) {
            bad++;
        } else {
            next[k]++;
        }
        got++;
    }
    for (int k = 0; k < W; k++) {
        waitpid(kids[k], NULL, 0);
    }
    check(got == W * L && bad == 0, "ctl: 4 senders x 200 commands: all arrive, whole, in order per sender",
          "lines were lost, mixed up or cut");

    step("ctl: close", 5);
    chan_close(r);
    check(!is_fifo || access(path, F_OK) != 0, "ctl: the receiver's chan_close removes the FIFO", path);
    s = chan_open(addr, CHAN_SEND);
    check(!s, "ctl: after the receiver is gone, a sender fails at once", "");
    chan_close(s);
    alarm(0);
}

/* ================================================================================ file */

static void test_file(void) {
    char path[128], addr[160];
    snprintf(path, sizeof(path), "/tmp/tc-%d.rec", (int)getpid());
    snprintf(addr, sizeof(addr), "file:%s", path);
    static Frame f, g;

    step("file: the recorder creates the file", 5);
    chan_t *s = chan_open(addr, CHAN_SEND);
    check(s && access(path, F_OK) == 0, "file: the recorder creates the file", "chan_open returned NULL");
    if (!s) {
        return;
    }
    int ok = 1;
    for (int b = 0; b < 30; b++) {
        make_frame(&f, b);
        f.tempo_bpm = 120;
        frame_seal(&f);
        ok &= chan_send(s, &f, sizeof(f)) == 0;
    }
    check(ok, "file: 30 frames recorded", "");
    static char small[100];
    check(chan_send(s, small, sizeof(small)) == CHAN_TOOBIG, "file: a message that is not a Frame is refused", "");

    step("file: replay while recording", 5);
    chan_t *r = chan_open(addr, CHAN_RECV);
    int good = 0;
    for (int b = 0; r && b < 30; b++) {
        ssize_t n = chan_recv(r, &g, sizeof(g), 0);
        good += n == (ssize_t)sizeof(g) && frame_intact(&g) && g.beat == b;
    }
    ssize_t n = r ? chan_recv(r, &g, sizeof(g), 0) : -9;
    check(r && good == 30 && n == CHAN_TIMEOUT,
          "file: a replay of an unfinished recording gets the 30 frames, then CHAN_TIMEOUT", "");
    for (int b = 30; b < 40; b++) {
        make_frame(&f, b);
        chan_send(s, &f, sizeof(f));
    }
    good = 0;
    for (int b = 30; r && b < 40; b++) {
        n = chan_recv(r, &g, sizeof(g), 0);
        good += n == (ssize_t)sizeof(g) && frame_intact(&g) && g.beat == b;
    }
    check(good == 10, "file: ... and the frames recorded after it opened", "");

    step("file: close", 5);
    chan_close(s);
    struct stat st;
    stat(path, &st);
    RecHeader h;
    memset(&h, 0, sizeof(h));
    int fd = open(path, O_RDONLY);
    if (fd < 0 || read(fd, &h, sizeof(h)) != (ssize_t)sizeof(h)) {
        memset(&h, 0, sizeof(h));
    }
    if (fd >= 0) {
        close(fd);
    }
    check((size_t)st.st_size == REC_DATA_OFFSET + 40 * sizeof(Frame) && h.n_frames == 40 && h.complete == 1 &&
          !memcmp(h.magic, REC_MAGIC, 8) && h.version == REC_VERSION && h.frame_size == sizeof(Frame) &&
          h.tempo_bpm == 120,
          "file: after close the file holds the header (complete = 1) and exactly 40 frames",
          "wrong size or header (see recording.h)");
    n = r ? chan_recv(r, &g, sizeof(g), 0) : -9;
    check(n == CHAN_CLOSED, "file: the replay ends with CHAN_CLOSED once the recording is complete", "");
    chan_close(r);
    r = chan_open(addr, CHAN_RECV);
    good = 0;
    while (r && chan_recv(r, &g, sizeof(g), 0) == (ssize_t)sizeof(g)) {
        good += frame_intact(&g) && g.beat == good;
    }
    check(good == 40, "file: a new replay reads the 40 frames back, in order", "");
    chan_close(r);

    step("file: bad files", 5);
    char bad[160], badaddr[200];
    snprintf(bad, sizeof(bad), "/tmp/tc-%d.bad", (int)getpid());
    snprintf(badaddr, sizeof(badaddr), "file:%s", bad);
    FILE *fp = fopen(bad, "w");
    if (fp) {
        fputs("this is not a recording, but it is long enough to have a header......\n", fp);
        fclose(fp);
    }
    chan_t *rb = chan_open(badaddr, CHAN_RECV);
    check(!rb, "file: a file that is not a recording is refused (NULL, no crash)", "");
    chan_close(rb);
    if (truncate(path, (off_t)(REC_DATA_OFFSET + 10 * sizeof(Frame))) == 0) {
        rb = chan_open(addr, CHAN_RECV);
        check(!rb, "file: a truncated recording is refused (NULL, no crash)", "");
        chan_close(rb);
    }
    fp = fopen(bad, "w");
    if (fp) {
        fclose(fp);
    }
    rb = chan_open(badaddr, CHAN_RECV);
    check(!rb, "file: an empty file is refused (NULL, no crash)", "");
    chan_close(rb);
    unlink(bad);

    step("file: a long recording", 20);
    s = chan_open(addr, CHAN_SEND);
    int accepted = 0;
    for (int b = 0; s && b < 5000; b++) {
        make_frame(&f, b);
        if (chan_send(s, &f, sizeof(f)) == 0) {
            accepted++;
        }
    }
    chan_close(s);
    stat(path, &st);
    check(s && accepted > 100 && (size_t)st.st_size == REC_DATA_OFFSET + (size_t)accepted * sizeof(Frame),
          "file: 5000 frames: no crash, and the file holds exactly the frames accepted", "");
    unlink(path);
    alarm(0);
}

/* ================================================================================ tcp */

/* a plain socket client, written without the channel API, to feed the receiver odd pieces */
static int raw_connect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd >= 0 && connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void *recv_forever(void *arg) {
    char buf[64];
    long r = (long)chan_recv((chan_t *)arg, buf, sizeof(buf), -1);
    return (void *)r;
}

static void test_tcp(void) {
    char addr[64], buf[MAX_LINE];
    step("tcp: listen", 5);
    chan_t *l = chan_listen("tcp::0");
    int port = chan_addr(l, CHAN_LOCAL, NULL, 0);
    check(l && port > 0, "tcp: chan_listen(\"tcp::0\") listens on a free port (chan_addr tells which)",
          l ? "chan_addr returned no port" : "chan_listen returned NULL");
    if (!l || port <= 0) {
        return;
    }
    snprintf(addr, sizeof(addr), "tcp:127.0.0.1:%d", port);

    step("tcp: connect and accept", 5);
    chan_t *c = chan_open(addr, CHAN_BOTH);
    chan_t *s = c ? chan_accept(l, 2000) : NULL;
    check(c && s, "tcp: chan_open connects, chan_accept returns the new connection",
          c ? "chan_accept returned NULL" : "chan_open(tcp:127.0.0.1:PORT, CHAN_BOTH) returned NULL");
    if (!c || !s) {
        chan_close(c);
        chan_close(l);
        return;
    }
    check(fds_cloexec("socket:") == 1, "tcp: every socket is close-on-exec (SOCK_CLOEXEC)",
          "the center forks captains: they would inherit the sockets");
    char host[64] = "";
    int rport = chan_addr(s, CHAN_REMOTE, host, sizeof(host));
    check(rport > 0 && strstr(host, "127.0.0.1"), "tcp: chan_addr(CHAN_REMOTE) names the client", host);

    step("tcp: lines both ways", 5);
    ssize_t n1 = chan_send(c, "HELLO 1 16", 10) == 0 ? chan_recv(s, buf, sizeof(buf), 1000) : -9;
    int ok1 = n1 == 10 && !strcmp(buf, "HELLO 1 16");
    ssize_t n2 = chan_send(s, "ASSIGN 1 16 0 31 35 16", 22) == 0 ? chan_recv(c, buf, sizeof(buf), 1000) : -9;
    check(ok1 && n2 == 22 && !strcmp(buf, "ASSIGN 1 16 0 31 35 16"),
          "tcp: a line goes each way, whole, without its newline", "");

    step("tcp: timeout", 5);
    long t0 = now_ms(), c0 = cpu_ms();
    ssize_t r = chan_recv(s, buf, sizeof(buf), 300);
    long dt = now_ms() - t0;
    check(r == CHAN_TIMEOUT && dt >= 250 && dt < 1500 && cpu_ms() - c0 < 60,
          "tcp: nothing to read -> CHAN_TIMEOUT after about timeout_ms, without spinning", "");
    t0 = now_ms();
    chan_t *none = chan_accept(l, 300);
    dt = now_ms() - t0;
    check(!none && dt >= 250 && dt < 1500, "tcp: chan_accept with nobody calling returns NULL after timeout_ms", "");
    chan_close(none);

    step("tcp: odd pieces", 10);
    int raw = raw_connect(port);
    chan_t *s2 = raw >= 0 ? chan_accept(l, 2000) : NULL;
    if (raw >= 0 && s2) {
        const char *one = "BEAT 12\n";
        for (const char *p = one; *p; p++) {         /* one byte per write, with pauses */
            if (write(raw, p, 1) != 1) {
                break;
            }
            usleep(2000);
        }
        r = chan_recv(s2, buf, sizeof(buf), 2000);
        check(r == 7 && !strcmp(buf, "BEAT 12"), "tcp: a line written one byte at a time arrives whole", buf);
        const char *three = "BEAT 1\nBEAT 2\nBEAT 3\n";
        int got = write(raw, three, strlen(three)) == (ssize_t)strlen(three);
        for (int i = 1; i <= 3 && got; i++) {
            char want[16];
            snprintf(want, sizeof(want), "BEAT %d", i);
            got = chan_recv(s2, buf, sizeof(buf), 1000) >= 0 && !strcmp(buf, want);
        }
        check(got, "tcp: three lines in one write arrive as three messages", "");
        static char lng[MAX_LINE * 3];
        memset(lng, 'A', sizeof(lng) - 1);
        lng[sizeof(lng) - 2] = '\n';
        lng[sizeof(lng) - 1] = '\0';
        got = write(raw, lng, strlen(lng)) == (ssize_t)strlen(lng) && write(raw, "SHUTDOWN\n", 9) == 9;
        r = got ? chan_recv(s2, buf, sizeof(buf), 1000) : -9;
        ssize_t r2 = chan_recv(s2, buf, sizeof(buf), 1000);
        check(r == CHAN_TOOBIG && r2 == 8 && !strcmp(buf, "SHUTDOWN"),
              "tcp: a too long line gives CHAN_TOOBIG, and the next line is fine", "");
        close(raw);
        r = chan_recv(s2, buf, sizeof(buf), 1000);
        check(r == CHAN_CLOSED, "tcp: the peer closes -> CHAN_CLOSED", "");
        int sent_fail = 0;
        for (int i = 0; i < 200 && !sent_fail; i++) {
            sent_fail = chan_send(s2, "BEAT 4", 6) == CHAN_CLOSED;
            usleep(1000);
        }
        check(sent_fail, "tcp: sending to a closed peer gives CHAN_CLOSED (and no SIGPIPE death)", "");
    } else {
        check(0, "tcp: a plain client connects", "raw connect or chan_accept failed");
    }
    chan_close(s2);

    step("tcp: closed port", 5);
    chan_t *l2 = chan_listen("tcp::0");
    int dead_port = chan_addr(l2, CHAN_LOCAL, NULL, 0);
    chan_close(l2);
    char dead[64];
    snprintf(dead, sizeof(dead), "tcp:127.0.0.1:%d", dead_port);
    t0 = now_ms();
    chan_t *nobody = chan_open(dead, CHAN_BOTH);
    check(!nobody && now_ms() - t0 < 1000, "tcp: connecting to a port where nobody listens fails at once", "");
    chan_close(nobody);

    step("tcp: chan_shutdown", 5);
    pthread_t th;
    pthread_create(&th, NULL, recv_forever, s);
    usleep(100000);
    t0 = now_ms();
    chan_shutdown(s);
    void *res;
    pthread_join(th, &res);
    check((long)res == CHAN_CLOSED && now_ms() - t0 < 1000,
          "tcp: chan_shutdown wakes a thread blocked in chan_recv(-1) with CHAN_CLOSED", "");
    chan_close(s);
    chan_close(c);

    step("tcp: many clients", 15);
    enum { NC = 20, NL = 50 };
    chan_t *cl[NC], *sv[NC];
    int all = 1;
    for (int i = 0; i < NC; i++) {
        cl[i] = chan_open(addr, CHAN_BOTH);
        all &= cl[i] != NULL;
    }
    for (int i = 0; i < NC && all; i++) {
        for (int k = 0; k < NL; k++) {
            snprintf(buf, sizeof(buf), "POS %d %d 0 1 2 3", i % MAX_LEADERS, k);
            all &= chan_send(cl[i], buf, strlen(buf)) == 0;
        }
    }
    for (int i = 0; i < NC; i++) {
        sv[i] = all ? chan_accept(l, 2000) : NULL;
        all &= sv[i] != NULL;
    }
    int in_order = all;
    for (int i = 0; i < NC && all; i++) {             /* the connections come in the order they connected */
        int who = -1;
        for (int k = 0; k < NL && in_order; k++) {
            int lid, beat;
            in_order = chan_recv(sv[i], buf, sizeof(buf), 1000) > 0 &&
                       sscanf(buf, "POS %d %d", &lid, &beat) == 2 && beat == k;
            who = lid;
        }
        (void)who;
    }
    check(all && in_order, "tcp: 20 clients x 50 lines: all accepted, every line whole and in order", "");
    for (int i = 0; i < NC; i++) {
        chan_close(cl[i]);
        chan_close(sv[i]);
    }

    step("tcp: a client that does not read", 40);
    c = chan_open(addr, CHAN_BOTH);
    s = c ? chan_accept(l, 2000) : NULL;
    if (s) {
        static char big[3000];
        memset(big, 'D', sizeof(big));
        long worst = 0;
        int result = 0, i;
        for (i = 0; i < 100000 && result == 0; i++) {
            long t = now_ms();
            result = chan_send(s, big, sizeof(big));
            if (now_ms() - t > worst) {
                worst = now_ms() - t;
            }
        }
        /* SO_SNDTIMEO of 2 s: a message split over two write()s may wait twice */
        check(result == CHAN_CLOSED && worst < 6000,
              "tcp: sending to a peer that never reads fails (CHAN_CLOSED) within a few seconds",
              result ? "a send waited 6 s or more" : "100000 messages were accepted: no limit at all?");
        printf("      (gave up after %d messages; the longest chan_send took %ld ms)\n", i, worst);
    }
    chan_close(s);
    chan_close(c);
    chan_close(l);
    alarm(0);
}

/* ================================================================================ udp */

typedef struct { chan_t *peer; int n; } PeerJob;

static void *send_many(void *arg) {
    PeerJob *j = arg;
    char m[32];
    for (int i = 0; i < j->n; i++) {
        snprintf(m, sizeof(m), "GOTO 1 %d 2 3 4", i);
        chan_send(j->peer, m, strlen(m));
    }
    return NULL;
}

static void test_udp(void) {
    char addr[64], buf[MAX_LINE];
    step("udp: a radio port", 5);
    unsetenv("SWARM_UDP_LOSS");
    chan_t *srv = chan_open("udp::0", CHAN_BOTH);
    int port = chan_addr(srv, CHAN_LOCAL, NULL, 0);
    check(srv && port > 0, "udp: chan_open(\"udp::0\") opens a free port (chan_addr tells which)", "");
    if (!srv || port <= 0) {
        chan_close(srv);
        return;
    }
    check(chan_peer(srv) == NULL, "udp: chan_peer before anything was received is NULL", "");
    snprintf(addr, sizeof(addr), "udp:127.0.0.1:%d", port);
    chan_t *a = chan_open(addr, CHAN_BOTH), *b = chan_open(addr, CHAN_BOTH);
    check(a && b, "udp: clients open udp:127.0.0.1:PORT", "");
    if (!a || !b) {
        chan_close(a);
        chan_close(b);
        chan_close(srv);
        return;
    }
    check(fds_cloexec("socket:") == 1, "udp: every socket is close-on-exec", "");

    step("udp: one message", 5);
    ssize_t r = chan_send(a, "NEXT 3 0", 8) == 0 ? chan_recv(srv, buf, sizeof(buf), 1000) : -9;
    check(r == 8 && !strcmp(buf, "NEXT 3 0"), "udp: a datagram arrives whole, as one message", "");
    chan_t *pa = chan_peer(srv);
    chan_send(b, "NEXT 4 0", 8);
    r = chan_recv(srv, buf, sizeof(buf), 1000);
    chan_t *pb = chan_peer(srv);
    check(r == 8 && pa && pb, "udp: a second client is heard; chan_peer gives a channel to each sender", "");
    if (!pa || !pb) {
        chan_close(pa);
        chan_close(pb);
        chan_close(a);
        chan_close(b);
        chan_close(srv);
        return;
    }
    chan_send(pa, "WAIT 3 0", 8);
    chan_send(pb, "WAIT 4 0", 8);
    ssize_t ra = chan_recv(a, buf, sizeof(buf), 1000);
    int oka = ra == 8 && !strcmp(buf, "WAIT 3 0");
    ssize_t rb = chan_recv(b, buf, sizeof(buf), 1000);
    int okb = rb == 8 && !strcmp(buf, "WAIT 4 0");
    check(oka && okb, "udp: each peer channel answers its own client, and only that one", "");

    step("udp: boundaries", 5);
    chan_send(a, "AT 3 0 1 1", 10);
    chan_send(a, "AT 3 1 1 2", 10);
    chan_send(a, "AT 3 2 1 3", 10);
    int three = 1;
    for (int i = 0; i < 3; i++) {
        char want[16];
        snprintf(want, sizeof(want), "AT 3 %d 1 %d", i, i + 1);
        three &= chan_recv(srv, buf, sizeof(buf), 1000) == 10 && !strcmp(buf, want);
    }
    check(three, "udp: three datagrams are three messages (no merging, no splitting)", "");
    static char big[600];
    memset(big, 'x', sizeof(big));
    check(chan_send(a, big, sizeof(big)) == CHAN_TOOBIG, "udp: a message over 512 bytes is refused", "");
    chan_send(a, "GOTO 3 0 10 11 12", 17);
    chan_send(a, "LAND 3", 6);
    r = chan_recv(srv, buf, 8, 1000);
    ssize_t r2 = chan_recv(srv, buf, sizeof(buf), 1000);
    check(r == CHAN_TOOBIG && r2 == 6 && !strcmp(buf, "LAND 3"),
          "udp: a datagram bigger than the buffer gives CHAN_TOOBIG, the next one is fine", "");

    step("udp: timeout", 5);
    long t0 = now_ms(), c0 = cpu_ms();
    r = chan_recv(srv, buf, sizeof(buf), 300);
    long dt = now_ms() - t0;
    check(r == CHAN_TIMEOUT && dt >= 250 && dt < 1500 && cpu_ms() - c0 < 60,
          "udp: nothing -> CHAN_TIMEOUT after about timeout_ms, without spinning", "");

    step("udp: a dead captain", 5);
    chan_t *tmp = chan_open("udp::0", CHAN_BOTH);
    int dead_port = chan_addr(tmp, CHAN_LOCAL, NULL, 0);
    chan_close(tmp);
    char dead[64];
    snprintf(dead, sizeof(dead), "udp:127.0.0.1:%d", dead_port);
    chan_t *lonely = chan_open(dead, CHAN_BOTH);
    t0 = now_ms();
    int s1 = lonely ? chan_send(lonely, "NEXT 1 0", 8) : -9;
    r = lonely ? chan_recv(lonely, buf, sizeof(buf), 500) : -9;
    dt = now_ms() - t0;
    check(lonely && (s1 == 0 || s1 == CHAN_CLOSED) && (r == CHAN_CLOSED || r == CHAN_TIMEOUT) && dt < 1000,
          "udp: talking to a port where nobody listens: no hang (CHAN_CLOSED or CHAN_TIMEOUT)", "");
    printf("      (got %s: %s)\n", r == CHAN_CLOSED ? "CHAN_CLOSED" : "CHAN_TIMEOUT",
           r == CHAN_CLOSED ? "the ICMP 'port unreachable' was reported" : "the error was not reported");
    chan_close(lonely);

    step("udp: threads share the socket", 10);
    PeerJob ja = { pa, 200 }, jb = { pb, 200 };
    pthread_t ta, tb;
    pthread_create(&ta, NULL, send_many, &ja);
    pthread_create(&tb, NULL, send_many, &jb);
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    int na = 0, nb = 0, whole = 1;
    while (chan_recv(a, buf, sizeof(buf), 200) > 0) {
        na++;
        whole &= !strncmp(buf, "GOTO 1 ", 7);
    }
    while (chan_recv(b, buf, sizeof(buf), 200) > 0) {
        nb++;
        whole &= !strncmp(buf, "GOTO 1 ", 7);
    }
    check(whole && na >= 190 && nb >= 190 && na <= 200 && nb <= 200,
          "udp: two threads x 200 sends on two peer channels of one socket: whole, to the right client", "");
    chan_close(pa);
    chan_send(a, "NEXT 3 9", 8);
    r = chan_recv(srv, buf, sizeof(buf), 1000);
    check(r == 8, "udp: closing a peer channel does not close the shared socket", "");
    chan_close(pb);

    step("udp: SWARM_UDP_LOSS", 10);
    setenv("SWARM_UDP_LOSS", "30", 1);
    chan_t *noisy = chan_open(addr, CHAN_BOTH);
    unsetenv("SWARM_UDP_LOSS");
    int got = 0;
    for (int i = 0; noisy && i < 1000; i++) {
        chan_send(noisy, "NEXT 2 0", 8);
        if (i % 50 == 49) {
            while (chan_recv(srv, buf, sizeof(buf), 0) > 0) {
                got++;
            }
        }
    }
    while (chan_recv(srv, buf, sizeof(buf), 100) > 0) {
        got++;
    }
    check(noisy && got >= 550 && got <= 850,
          "udp: SWARM_UDP_LOSS=30 drops about 30% of the datagrams a channel sends (for the tests)", "");
    printf("      (%d of 1000 arrived)\n", got);
    chan_close(noisy);
    chan_close(a);
    chan_close(b);
    chan_close(srv);
    alarm(0);
}

int main(int argc, char **argv) {
    signal(SIGALRM, on_alarm);
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *what = argc > 1 ? argv[1] : "all";
    if (!strcmp(what, "shm") || !strcmp(what, "all")) {
        test_shm();
    }
    if (!strcmp(what, "fifo") || !strcmp(what, "ctl") || !strcmp(what, "all")) {
        test_fifo();
    }
    if (!strcmp(what, "file") || !strcmp(what, "all")) {
        test_file();
    }
    if (!strcmp(what, "tcp") || !strcmp(what, "net")) {
        test_tcp();
    }
    if (!strcmp(what, "udp") || !strcmp(what, "net")) {
        test_udp();
    }
    return fails;
}
