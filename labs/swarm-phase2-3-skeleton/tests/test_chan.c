/* test_chan.c — tests of YOUR channel API against PROTOCOL.md Section 1 (given).
 *
 *   make test                  every scheme
 *   ./bin/test_chan local tcp  only these (local, tcp, udp, shm, file)
 *
 * Each check prints PASS or FAIL with what it expected. The exit status is the number of failures
 * (0: all passed). The test uses only the seven functions of include/chan.h.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "chan.h"
#include "swarm.h"

static int failures, passes;
static char sock_path[108], shm_name[64], rec_path[128];

static void check(int ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("%s  ", ok ? "PASS" : "FAIL");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    fflush(stdout);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int send_str(chan_t *c, const char *s) {
    return chan_send(c, s, strlen(s));
}

/* receive one message and compare it with want */
static int got(chan_t *c, const char *want, int timeout_ms) {
    char buf[1024];
    ssize_t r = chan_recv(c, buf, sizeof(buf), timeout_ms);
    return r == (ssize_t)strlen(want) && !strcmp(buf, want);
}

/* ------------------------------------------------------------------------ line schemes */

struct reader {
    chan_t *c;
    int     n;                      /* > 0: n POS lines; < 0: a PLAN of -n keyframes */
    int     ok;
};

static void *read_lines(void *arg) {
    struct reader *r = arg;
    char want[32];
    r->ok = 1;
    if (r->n < 0) {
        r->ok = got(r->c, "PLAN 7 4000", 3000);
        for (int i = 0; i < -r->n && r->ok; i++) {
            snprintf(want, sizeof(want), "%d %d 10 20 5", i / 64, i % 64);
            r->ok = got(r->c, want, 3000);
        }
        return NULL;
    }
    for (int i = 0; i < r->n && r->ok; i++) {
        snprintf(want, sizeof(want), "POS %d 0 1 2 3", i);
        r->ok = got(r->c, want, 3000);
    }
    return NULL;
}

/* the checks shared by local: and tcp: on a connected pair (client a, server side b) */
static void line_pair(const char *scheme, chan_t *a, chan_t *b) {
    char buf[1024];
    check(send_str(a, "HELLO 0") == 0, "%s: chan_send of a line returns 0", scheme);
    ssize_t r = chan_recv(b, buf, sizeof(buf), 1000);
    check(r == 7 && !strcmp(buf, "HELLO 0"), "%s: the line arrives without its newline (got %zd '%s')",
          scheme, r, r >= 0 ? buf : "");
    check(send_str(b, "BEAT 1") == 0 && got(a, "BEAT 1", 1000), "%s: the other way too", scheme);

    send_str(a, "PLAN 1 2\n0 0 1 1 3\n0 1 2 1 3");
    int ok = got(b, "PLAN 1 2", 1000) && got(b, "0 0 1 1 3", 1000) && got(b, "0 1 2 1 3", 1000);
    check(ok, "%s: one send of three lines arrives as three messages", scheme);

    long t = now_ms();
    r = chan_recv(b, buf, sizeof(buf), 150);
    long dt = now_ms() - t;
    check(r == CHAN_TIMEOUT && dt >= 120 && dt < 1000, "%s: chan_recv times out (got %zd after %ld ms)",
          scheme, r, dt);
    check(chan_recv(b, buf, sizeof(buf), 0) == CHAN_TIMEOUT, "%s: timeout 0 does not wait", scheme);

    char big[300];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    send_str(a, big);
    send_str(a, "after");
    r = chan_recv(b, buf, 64, 1000);
    check(r == CHAN_TOOBIG, "%s: a line longer than the buffer is CHAN_TOOBIG (got %zd)", scheme, r);
    check(got(b, "after", 1000), "%s: ... and the next line arrives whole", scheme);

    /* a reader thread on one side while this thread sends: nothing lost, nothing reordered */
    struct reader rd = { b, 2000, 0 };
    pthread_t th;
    pthread_create(&th, NULL, read_lines, &rd);
    int sent = 1;
    for (int i = 0; i < 2000 && sent; i++) {
        char line[32];
        snprintf(line, sizeof(line), "POS %d 0 1 2 3", i);
        sent = send_str(a, line) == 0;
    }
    pthread_join(th, NULL);
    check(sent && rd.ok, "%s: 2000 lines arrive, all of them, in order", scheme);

    /* one big message of many lines, like a PLAN with its keyframes */
    size_t cap = 4000 * 24;
    char *plan = malloc(cap);
    size_t o = (size_t)snprintf(plan, cap, "PLAN 7 4000");
    for (int i = 0; i < 4000; i++) {
        o += (size_t)snprintf(plan + o, cap - o, "\n%d %d 10 20 5", i / 64, i % 64);
    }
    struct reader pr = { b, -4000, 0 };
    pthread_create(&th, NULL, read_lines, &pr);
    int s = chan_send(a, plan, o);
    pthread_join(th, NULL);
    check(s == 0, "%s: a 4001-line message can be sent", scheme);
    check(pr.ok, "%s: ... and all its lines arrive", scheme);
    free(plan);
}

static void line_close(const char *scheme, chan_t *a, chan_t *b) {
    char buf[64];
    send_str(a, "last words");
    chan_close(a);
    check(got(b, "last words", 1000), "%s: a line sent just before close still arrives", scheme);
    ssize_t r = chan_recv(b, buf, sizeof(buf), 1000);
    check(r == CHAN_CLOSED, "%s: then chan_recv says CHAN_CLOSED (got %zd)", scheme, r);
    int s = 0;
    for (int i = 0; i < 50 && s == 0; i++) {            /* the first sends may still succeed */
        s = send_str(b, "anyone there?");
        sleep_ms(10);
    }
    check(s == CHAN_CLOSED, "%s: sending to a closed peer returns CHAN_CLOSED, without SIGPIPE", scheme);
    chan_close(b);
}

static void test_local(void) {
    char addr[160];
    snprintf(addr, sizeof(addr), "local:%s", sock_path);
    errno = 0;
    chan_t *none = chan_open(addr, CHAN_BOTH);
    check(none == NULL && (errno == ENOENT || errno == ECONNREFUSED),
          "local: connecting with no server fails with ENOENT or ECONNREFUSED (errno %d)", errno);
    chan_close(none);

    chan_t *srv = chan_listen(addr);
    check(srv != NULL, "local: chan_listen(%s)", addr);
    if (!srv) {
        return;
    }
    check(exists(sock_path), "local: the socket file exists");
    long t = now_ms();
    chan_t *nobody = chan_accept(srv, 150);
    check(nobody == NULL && now_ms() - t >= 120, "local: chan_accept with nobody returns NULL after the timeout");
    chan_close(nobody);

    chan_t *a = chan_open(addr, CHAN_BOTH);
    chan_t *b = chan_accept(srv, 1000);
    check(a && b, "local: chan_open connects and chan_accept returns the client");
    if (a && b) {
        check(chan_addr(b, CHAN_REMOTE, NULL, 0) == 0, "local: chan_addr returns 0");
        line_pair("local", a, b);
        line_close("local", a, b);
    }

    chan_t *cl[20], *sv[20];
    int n = 0;
    for (int i = 0; i < 20; i++) {
        cl[i] = chan_open(addr, CHAN_BOTH);
        char line[16];
        snprintf(line, sizeof(line), "HELLO %d", i);
        if (cl[i]) {
            send_str(cl[i], line);
        }
    }
    int seen[20] = { 0 };
    for (int i = 0; i < 20; i++) {
        sv[i] = chan_accept(srv, 1000);
        char buf[32];
        int k;
        if (sv[i] && chan_recv(sv[i], buf, sizeof(buf), 1000) > 0 && sscanf(buf, "HELLO %d", &k) == 1 &&
            k >= 0 && k < 20 && !seen[k]) {
            seen[k] = 1;
            n++;
        }
    }
    check(n == 20, "local: 20 clients at once are each accepted with their own channel (%d)", n);
    for (int i = 0; i < 20; i++) {
        chan_close(cl[i]);
        chan_close(sv[i]);
    }
    chan_close(srv);
    check(!exists(sock_path), "local: closing the server removes the socket file");

    /* a server that died without closing leaves a stale file: the next one must still start */
    pid_t p = fork();
    if (p == 0) {
        chan_listen(addr);
        _exit(0);                                       /* no chan_close: the file stays */
    }
    waitpid(p, NULL, 0);
    srv = chan_listen(addr);
    check(srv != NULL, "local: chan_listen works over a stale socket file");
    chan_close(srv);
}

static void test_tcp(void) {
    chan_t *srv = chan_listen("tcp:127.0.0.1:0");
    check(srv != NULL, "tcp: chan_listen(tcp:127.0.0.1:0)");
    if (!srv) {
        return;
    }
    char host[64] = "";
    int port = chan_addr(srv, CHAN_LOCAL, host, sizeof(host));
    check(port > 0 && !strcmp(host, "127.0.0.1"), "tcp: chan_addr gives the port it got (%s:%d)", host, port);
    char addr[96];
    snprintf(addr, sizeof(addr), "tcp:127.0.0.1:%d", port);
    chan_t *a = chan_open(addr, CHAN_BOTH);
    chan_t *b = chan_accept(srv, 1000);
    check(a && b, "tcp: connect and accept");
    if (a && b) {
        char rh[64] = "";
        int rp = chan_addr(b, CHAN_REMOTE, rh, sizeof(rh));
        int lp = chan_addr(a, CHAN_LOCAL, NULL, 0);
        check(rp > 0 && rp == lp && !strcmp(rh, "127.0.0.1"),
              "tcp: chan_addr(CHAN_REMOTE) names the client (%s:%d)", rh, rp);
        line_pair("tcp", a, b);
        line_close("tcp", a, b);
    }
    chan_close(srv);

    errno = 0;
    long t = now_ms();
    chan_t *none = chan_open(addr, CHAN_BOTH);
    check(none == NULL && errno == ECONNREFUSED && now_ms() - t < 3000,
          "tcp: connecting to a closed port fails at once with ECONNREFUSED (errno %d)", errno);
    chan_close(none);

    srv = chan_listen("tcp::0");
    port = srv ? chan_addr(srv, CHAN_LOCAL, NULL, 0) : -1;
    snprintf(addr, sizeof(addr), "tcp:127.0.0.1:%d", port);
    a = srv ? chan_open(addr, CHAN_BOTH) : NULL;
    b = srv ? chan_accept(srv, 1000) : NULL;
    check(a && b, "tcp: tcp::0 listens on every address (reached on 127.0.0.1:%d)", port);
    chan_close(a);
    chan_close(b);
    chan_close(srv);

    /* SWARM_BIND: one machine, many addresses */
    srv = chan_listen("tcp:127.0.0.1:0");
    port = srv ? chan_addr(srv, CHAN_LOCAL, NULL, 0) : -1;
    snprintf(addr, sizeof(addr), "tcp:127.0.0.1:%d", port);
    setenv("SWARM_BIND", "127.0.0.7", 1);
    a = srv ? chan_open(addr, CHAN_BOTH) : NULL;
    chan_t *l2 = chan_listen("tcp::0");
    unsetenv("SWARM_BIND");
    b = srv ? chan_accept(srv, 1000) : NULL;
    host[0] = '\0';
    if (b) {
        chan_addr(b, CHAN_REMOTE, host, sizeof(host));
    }
    check(a && b && !strcmp(host, "127.0.0.7"), "tcp: with SWARM_BIND=127.0.0.7 a connection comes from it (%s)", host);
    host[0] = '\0';
    if (l2) {
        chan_addr(l2, CHAN_LOCAL, host, sizeof(host));
    }
    check(l2 && !strcmp(host, "127.0.0.7"), "tcp: with SWARM_BIND=127.0.0.7 tcp::0 listens on it (%s)", host);
    chan_close(a);
    chan_close(b);
    chan_close(l2);
    chan_close(srv);
}

/* -------------------------------------------------------------------------------- udp: */

static void test_udp(void) {
    char buf[1024];
    chan_t *srv = chan_listen("udp:127.0.0.1:0");
    check(srv != NULL, "udp: chan_listen(udp:127.0.0.1:0)");
    if (!srv) {
        return;
    }
    int port = chan_addr(srv, CHAN_LOCAL, NULL, 0);
    check(port > 0, "udp: chan_addr gives the port (%d)", port);
    char addr[96];
    snprintf(addr, sizeof(addr), "udp:127.0.0.1:%d", port);
    chan_t *a = chan_open(addr, CHAN_BOTH);
    chan_t *b = chan_open(addr, CHAN_BOTH);
    check(a && b, "udp: chan_open(%s)", addr);
    if (!a || !b) {
        chan_close(a);
        chan_close(b);
        chan_close(srv);
        return;
    }
    check(send_str(a, "NEXT 3 0") == 0, "udp: chan_send returns 0");
    ssize_t r = chan_recv(srv, buf, sizeof(buf), 1000);
    check(r == 8 && !strcmp(buf, "NEXT 3 0"), "udp: one datagram is one message, no newline added (%zd '%s')",
          r, r >= 0 ? buf : "");
    check(send_str(srv, "WAIT 3 0") == 0 && got(a, "WAIT 3 0", 1000),
          "udp: the server's chan_send answers the sender (the reply rule)");

    send_str(a, "NEXT 1 5");
    got(srv, "NEXT 1 5", 1000);
    send_str(b, "NEXT 2 5");
    got(srv, "NEXT 2 5", 1000);
    send_str(srv, "GOTO 2 5 1 1 1");
    check(got(b, "GOTO 2 5 1 1 1", 1000) && chan_recv(a, buf, sizeof(buf), 200) == CHAN_TIMEOUT,
          "udp: with two clients, the answer goes to the last sender only");
    int bp = chan_addr(b, CHAN_LOCAL, NULL, 0);
    check(chan_addr(srv, CHAN_REMOTE, NULL, 0) == bp, "udp: chan_addr(CHAN_REMOTE) on the server names the last sender");

    char big[600];
    memset(big, 'y', sizeof(big));
    check(chan_send(a, big, sizeof(big)) == CHAN_TOOBIG, "udp: a message over 512 bytes is CHAN_TOOBIG");
    chan_send(a, big, 100);
    r = chan_recv(srv, buf, 10, 1000);
    check(r == CHAN_TOOBIG, "udp: a datagram bigger than the buffer is CHAN_TOOBIG (got %zd)", r);
    long t = now_ms();
    r = chan_recv(srv, buf, sizeof(buf), 150);
    check(r == CHAN_TIMEOUT && now_ms() - t >= 120, "udp: chan_recv times out");

    setenv("SWARM_DROP", "100", 1);
    chan_t *lossy = chan_open(addr, CHAN_BOTH);
    int s = lossy ? send_str(lossy, "NEXT 0 9") : -9;
    unsetenv("SWARM_DROP");
    check(lossy && s == 0 && chan_recv(srv, buf, sizeof(buf), 200) == CHAN_TIMEOUT,
          "udp: SWARM_DROP=100 drops every datagram, and chan_send still returns 0");
    chan_close(lossy);

    chan_close(a);
    chan_close(b);
    chan_close(srv);

    a = chan_open(addr, CHAN_BOTH);                     /* nobody listens there any more */
    if (a) {
        send_str(a, "NEXT 0 0");
        r = chan_recv(a, buf, sizeof(buf), 500);
        check(r == CHAN_CLOSED, "udp: on one machine, talking to a closed port gives CHAN_CLOSED (got %zd)", r);
        chan_close(a);
    } else {
        check(0, "udp: chan_open works even if nobody listens yet");
    }

    setenv("SWARM_BIND", "127.0.0.8", 1);
    srv = chan_listen("udp::0");
    unsetenv("SWARM_BIND");
    char host[64] = "";
    port = srv ? chan_addr(srv, CHAN_LOCAL, host, sizeof(host)) : -1;
    check(srv && !strcmp(host, "127.0.0.8"), "udp: with SWARM_BIND=127.0.0.8 udp::0 listens on it (%s)", host);
    snprintf(addr, sizeof(addr), "udp:127.0.0.8:%d", port);
    setenv("SWARM_BIND", "127.0.0.9", 1);
    a = srv ? chan_open(addr, CHAN_BOTH) : NULL;
    unsetenv("SWARM_BIND");
    host[0] = '\0';
    if (a && send_str(a, "NEXT 0 0") == 0 && got(srv, "NEXT 0 0", 1000)) {
        chan_addr(srv, CHAN_REMOTE, host, sizeof(host));
    }
    check(!strcmp(host, "127.0.0.9"), "udp: with SWARM_BIND=127.0.0.9 datagrams come from it (%s)", host);
    chan_close(a);
    chan_close(srv);
}

/* -------------------------------------------------------------------------------- shm: */

static void fill(Frame *f, int beat) {
    memset(f, 0, sizeof(*f));
    f->magic = FRAME_MAGIC;
    f->beat = beat;
    memset(f->drones, beat & 0xff, sizeof(f->drones));
}

static int whole(const Frame *f) {                      /* every byte of the drones matches beat */
    const unsigned char *p = (const unsigned char *)f->drones;
    for (size_t i = 0; i < sizeof(f->drones); i++) {
        if (p[i] != (unsigned char)(f->beat & 0xff)) {
            return 0;
        }
    }
    return f->magic == FRAME_MAGIC;
}

static void test_shm(void) {
    char addr[96], path[128];
    snprintf(addr, sizeof(addr), "shm:%s", shm_name);
    snprintf(path, sizeof(path), "/dev/shm/%s", shm_name);
    Frame *f = malloc(sizeof(Frame)), *g = malloc(sizeof(Frame));

    errno = 0;
    chan_t *none = chan_open(addr, CHAN_RECV);
    check(none == NULL && errno == ENOENT, "shm: opening a bus that does not exist fails with ENOENT (errno %d)", errno);
    chan_close(none);

    chan_t *tx = chan_open(addr, CHAN_SEND);
    check(tx != NULL && exists(path), "shm: CHAN_SEND creates %s", path);
    if (!tx) {
        free(f);
        free(g);
        return;
    }
    chan_t *rx = chan_open(addr, CHAN_RECV);
    check(rx != NULL, "shm: CHAN_RECV opens it");
    check(rx && chan_recv(rx, g, sizeof(Frame), 0) == CHAN_TIMEOUT, "shm: nothing sent yet: CHAN_TIMEOUT");

    fill(f, 1);
    check(chan_send(tx, f, sizeof(Frame)) == 0, "shm: chan_send of a Frame returns 0");
    ssize_t r = rx ? chan_recv(rx, g, sizeof(Frame), 1000) : -9;
    check(r == (ssize_t)sizeof(Frame) && !memcmp(f, g, sizeof(Frame)), "shm: the receiver gets it, byte for byte");

    for (int b = 2; b <= 4; b++) {
        fill(f, b);
        chan_send(tx, f, sizeof(Frame));
    }
    r = rx ? chan_recv(rx, g, sizeof(Frame), 1000) : -9;
    check(r == (ssize_t)sizeof(Frame) && g->beat == 4, "shm: the newest message wins (got beat %d)", g->beat);
    check(rx && chan_recv(rx, g, sizeof(Frame), 100) == CHAN_TIMEOUT, "shm: ... and it is returned only once");

    chan_t *late = chan_open(addr, CHAN_RECV);
    r = late ? chan_recv(late, g, sizeof(Frame), 1000) : -9;
    check(r == (ssize_t)sizeof(Frame) && g->beat == 4, "shm: a new receiver first gets the newest message");
    chan_close(late);

    long t = now_ms();
    for (int i = 0; i < 20000; i++) {
        fill(f, i);
        chan_send(tx, f, sizeof(Frame));
    }
    check(now_ms() - t < 3000, "shm: 20000 sends with nobody reading never wait (%ld ms)", now_ms() - t);
    if (rx) {
        chan_recv(rx, g, sizeof(Frame), 0);             /* catch up */
    }

    check(chan_send(tx, f, sizeof(Frame) + 1) == CHAN_TOOBIG, "shm: a message bigger than a Frame is CHAN_TOOBIG");

    chan_t *many[16];
    int opened = 0;
    for (int i = 0; i < 16; i++) {
        many[i] = chan_open(addr, CHAN_RECV);
        opened += many[i] != NULL;
    }
    fill(f, 77);
    chan_send(tx, f, sizeof(Frame));
    int all = 0;
    for (int i = 0; i < 16; i++) {
        all += many[i] && chan_recv(many[i], g, sizeof(Frame), 1000) == (ssize_t)sizeof(Frame) && g->beat == 77;
        chan_close(many[i]);
    }
    check(opened == 16 && all == 16, "shm: 16 receivers at once all get the message (%d of %d)", all, opened);

    /* torn copies: another process writes as fast as it can, we read as fast as we can */
    pid_t p = fork();
    if (p == 0) {
        long end = now_ms() + 1500;
        for (int i = 100; now_ms() < end; i++) {
            fill(f, i);
            chan_send(tx, f, sizeof(Frame));
        }
        _exit(0);
    }
    int frames = 0, torn = 0;
    long end = now_ms() + 1200;
    while (rx && now_ms() < end) {
        if (chan_recv(rx, g, sizeof(Frame), 100) == (ssize_t)sizeof(Frame)) {
            frames++;
            torn += !whole(g);
        }
    }
    waitpid(p, NULL, 0);
    check(frames > 100 && torn == 0, "shm: a racing writer never gives a torn copy (%d frames, %d torn)", frames, torn);

    fill(f, 999);
    chan_send(tx, f, sizeof(Frame));
    chan_close(tx);
    check(!exists(path), "shm: closing the sender removes %s", path);
    r = rx ? chan_recv(rx, g, sizeof(Frame), 1000) : -9;
    check(r == (ssize_t)sizeof(Frame) && g->beat == 999, "shm: after the sender closed, the last message still arrives");
    r = rx ? chan_recv(rx, g, sizeof(Frame), 1000) : -9;
    check(r == CHAN_CLOSED, "shm: then CHAN_CLOSED (got %zd)", r);
    chan_close(rx);
    free(f);
    free(g);
}

/* ------------------------------------------------------------------------------- file: */

static void test_file(void) {
    char addr[160];
    snprintf(addr, sizeof(addr), "file:%s", rec_path);
    unlink(rec_path);
    errno = 0;
    chan_t *none = chan_open(addr, CHAN_RECV);
    check(none == NULL && errno == ENOENT, "file: opening a missing recording fails with ENOENT (errno %d)", errno);
    chan_close(none);

    chan_t *w = chan_open(addr, CHAN_SEND);
    check(w != NULL, "file: CHAN_SEND creates %s", rec_path);
    if (!w) {
        return;
    }
    Frame *f = malloc(sizeof(Frame)), *g = malloc(sizeof(Frame));
    fill(f, 5);
    int s1 = chan_send(w, "abc", 3), s2 = chan_send(w, f, sizeof(Frame)), s3 = chan_send(w, "z", 1);
    chan_close(w);
    check(s1 == 0 && s2 == 0 && s3 == 0, "file: chan_send returns 0");

    struct stat st;
    size_t want = 4 + 3 + 4 + sizeof(Frame) + 4 + 1;
    check(stat(rec_path, &st) == 0 && (size_t)st.st_size == want,
          "file: the file is the messages, each with a 4-byte length (%ld bytes, want %zu)",
          (long)st.st_size, want);
    FILE *fp = fopen(rec_path, "rb");
    unsigned char head[7] = { 0 };
    if (fp) {
        if (fread(head, 1, 7, fp) != 7) {
            head[0] = 0xff;
        }
        fclose(fp);
    }
    check(!memcmp(head, "\x03\x00\x00\x00" "abc", 7), "file: the length is little-endian, then the bytes");

    chan_t *rd = chan_open(addr, CHAN_RECV);
    char buf[64];
    ssize_t r1 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    ssize_t r2 = rd ? chan_recv(rd, g, sizeof(Frame), 0) : -9;
    ssize_t r3 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    ssize_t r4 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    check(r1 == 3 && r2 == (ssize_t)sizeof(Frame) && !memcmp(f, g, sizeof(Frame)) && r3 == 1 && buf[0] == 'z',
          "file: CHAN_RECV returns the messages in order, from the start");
    check(r4 == CHAN_CLOSED, "file: then CHAN_CLOSED at the end (got %zd)", r4);
    chan_close(rd);

    rd = chan_open(addr, CHAN_RECV);
    r1 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    r2 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    r3 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
    check(r2 == CHAN_TOOBIG && r3 == 1, "file: a message bigger than the buffer is CHAN_TOOBIG, and the next one follows");
    chan_close(rd);

    if (truncate(rec_path, (off_t)(4 + 3 + 4 + 100)) == 0) {   /* a recorder that crashed mid-frame */
        rd = chan_open(addr, CHAN_RECV);
        r1 = rd ? chan_recv(rd, buf, sizeof(buf), 0) : -9;
        r2 = rd ? chan_recv(rd, g, sizeof(Frame), 0) : -9;
        check(r1 == 3 && r2 == CHAN_CLOSED, "file: a message cut short reads as the end");
        chan_close(rd);
    }

    w = chan_open(addr, CHAN_SEND);
    chan_close(w);
    check(stat(rec_path, &st) == 0 && st.st_size == 0, "file: CHAN_SEND truncates an old recording");
    unlink(rec_path);
    free(f);
    free(g);
}

/* --------------------------------------------------------------------------------- main */

int main(int argc, char **argv) {
    int pid = (int)getpid();
    snprintf(sock_path, sizeof(sock_path), "/tmp/swarm-test-%d.sock", pid);
    snprintf(shm_name, sizeof(shm_name), "swarm-test-%d", pid);
    snprintf(rec_path, sizeof(rec_path), "/tmp/swarm-test-%d.rec", pid);
    alarm(120);                                          /* a test that hangs is a failure */

    const char *all[] = { "local", "tcp", "udp", "shm", "file" };
    void (*fn[])(void) = { test_local, test_tcp, test_udp, test_shm, test_file };
    for (int i = 0; i < 5; i++) {
        int run = argc == 1;
        for (int k = 1; k < argc; k++) {
            run |= !strcmp(argv[k], all[i]);
        }
        if (run) {
            printf("---- %s:\n", all[i]);
            fflush(stdout);
            fn[i]();
        }
    }
    check(chan_open("nonsense", CHAN_SEND) == NULL, "an unknown address gives NULL");
    chan_close(NULL);
    printf("%d passed, %d failed\n", passes, failures);
    return failures > 255 ? 255 : failures;
}
