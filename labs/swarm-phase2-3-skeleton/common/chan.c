/* chan.c — the channel API: picks the transport from the address, and the fd transport (given).
 * The other transports are in chan_shm.c, chan_fifo.c, chan_mq.c and chan_file.c. */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "line_io.h"
#include "swarm.h"

/* ------------------------------------------------------------------------- the dispatcher */

chan_t *chan_open(const char *addr, int mode) {
    if (!addr || mode < CHAN_SEND || mode > CHAN_BOTH) {
        return NULL;
    }
    const char *colon = strchr(addr, ':');
    if (!colon || colon[1] == '\0') {
        fprintf(stderr, "chan: bad address '%s' (expected kind:name, e.g. shm:swarm-1000)\n", addr);
        return NULL;
    }
    size_t k = (size_t)(colon - addr);
    const char *rest = colon + 1;
    if (k == 3 && !strncmp(addr, "shm", 3))  return chan_shm_open(rest, mode);
    if (k == 4 && !strncmp(addr, "fifo", 4)) return chan_fifo_open(rest, mode);
    if (k == 2 && !strncmp(addr, "mq", 2))   return chan_mq_open(rest, mode);
    if (k == 4 && !strncmp(addr, "file", 4)) return chan_file_open(rest, mode);
    if (k == 3 && !strncmp(addr, "tcp", 3))  return chan_tcp_open(rest, mode);
    if (k == 3 && !strncmp(addr, "udp", 3))  return chan_udp_open(rest, mode);
    fprintf(stderr, "chan: unknown transport in '%s'\n", addr);
    return NULL;
}

chan_t *chan_listen(const char *addr) {
    if (!addr || strncmp(addr, "tcp:", 4) != 0) {
        fprintf(stderr, "chan: can only listen on tcp:[HOST]:PORT, not '%s'\n", addr ? addr : "");
        return NULL;
    }
    return chan_tcp_listen(addr + 4);
}

chan_t *chan_accept(chan_t *l, int timeout_ms) {
    return l && l->ops->accept ? l->ops->accept(l, timeout_ms) : NULL;
}

chan_t *chan_peer(chan_t *c) {
    return c && c->ops->peer ? c->ops->peer(c) : NULL;
}

int chan_addr(chan_t *c, int which, char *host, size_t n) {
    return c && c->ops->addr ? c->ops->addr(c, which, host, n) : -1;
}

void chan_shutdown(chan_t *c) {
    if (c && c->ops->shutdown) {
        c->ops->shutdown(c);
    }
}

int chan_split_hostport(const char *hp, char *host, size_t hn, char *port, size_t pn) {
    const char *colon = strrchr(hp, ':');
    if (!colon || colon[1] == '\0' || strlen(colon + 1) >= pn) {
        return -1;
    }
    size_t h = (size_t)(colon - hp);
    if (h >= 2 && hp[0] == '[' && hp[h - 1] == ']') {   /* [::1]:9000 */
        hp++;
        h -= 2;
    }
    if (h >= hn) {
        return -1;
    }
    memcpy(host, hp, h);
    host[h] = '\0';
    snprintf(port, pn, "%s", colon + 1);
    return 0;
}

int chan_sockaddr(int fd, int which, char *host, size_t n) {
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    int r = which == CHAN_REMOTE ? getpeername(fd, (struct sockaddr *)&ss, &len)
                                 : getsockname(fd, (struct sockaddr *)&ss, &len);
    char h[NI_MAXHOST], p[NI_MAXSERV];
    if (r != 0 || getnameinfo((struct sockaddr *)&ss, len, h, sizeof(h), p, sizeof(p),
                              NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return -1;
    }
    if (host) {
        snprintf(host, n, "%s", h);
    }
    return atoi(p);
}

int chan_send(chan_t *c, const void *msg, size_t len) {
    if (!c || !msg || !(c->mode & CHAN_SEND)) {
        return CHAN_CLOSED;
    }
    return c->ops->send(c, msg, len);
}

ssize_t chan_recv(chan_t *c, void *buf, size_t cap, int timeout_ms) {
    if (!c || !buf || cap == 0 || !(c->mode & CHAN_RECV)) {
        return CHAN_CLOSED;
    }
    return c->ops->recv(c, buf, cap, timeout_ms);
}

void chan_close(chan_t *c) {
    if (c) {
        c->ops->close(c);
    }
}

void ctl_addr(const char *name, char *addr, size_t n) {
    snprintf(addr, n, "%s:%s", CTL_TRANSPORT, name);
}

void chan_deadline(struct timespec *ts, int clock_id, int timeout_ms) {
    clock_gettime((clockid_t)clock_id, ts);
    ts->tv_sec += timeout_ms / 1000;
    ts->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* ------------------------------------------------------------- the fd transport: lines */

struct fd_chan {
    struct chan base;
    int         in_fd, out_fd;
    LineReader  lr;
};

static int fd_send(chan_t *c, const void *msg, size_t len) {
    struct fd_chan *f = (struct fd_chan *)c;
    char small[MAX_LINE + 1];
    char *buf = len + 1 <= sizeof(small) ? small : malloc(len + 1);
    if (!buf) {
        return CHAN_TOOBIG;
    }
    memcpy(buf, msg, len);
    buf[len] = '\n';
    int r = send_all(f->out_fd, buf, len + 1);       /* one write: lines never interleave */
    if (buf != small) {
        free(buf);
    }
    return r == 0 ? 0 : CHAN_CLOSED;
}

static ssize_t fd_recv(chan_t *c, void *buf, size_t cap, int timeout_ms) {
    struct fd_chan *f = (struct fd_chan *)c;
    if (timeout_ms >= 0 && f->lr.pos == f->lr.len) {  /* nothing buffered: wait for input */
        struct pollfd p = { .fd = f->in_fd, .events = POLLIN };
        int r;
        do {
            r = poll(&p, 1, timeout_ms);
        } while (r < 0 && errno == EINTR);
        if (r == 0) {
            return CHAN_TIMEOUT;
        }
    }
    int r = lr_read_line(&f->lr, buf, cap);
    if (r == 1) {
        return (ssize_t)strlen(buf);
    }
    return r == -2 ? CHAN_TOOBIG : CHAN_CLOSED;
}

static void fd_close(chan_t *c) {
    struct fd_chan *f = (struct fd_chan *)c;
    close(f->in_fd);
    if (f->out_fd != f->in_fd) {
        close(f->out_fd);
    }
    free(f);
}

static void fd_shutdown(chan_t *c) {
    struct fd_chan *f = (struct fd_chan *)c;
    shutdown(f->in_fd, SHUT_RDWR);                   /* a socket: wakes recv and send; a pipe: no-op */
    if (f->out_fd != f->in_fd) {
        shutdown(f->out_fd, SHUT_RDWR);
    }
}

static int fd_addr(chan_t *c, int which, char *host, size_t n) {
    return chan_sockaddr(((struct fd_chan *)c)->in_fd, which, host, n);
}

static const struct chan_ops fd_ops = { .send = fd_send, .recv = fd_recv, .close = fd_close,
                                        .shutdown = fd_shutdown, .addr = fd_addr };

chan_t *chan_fd(int in_fd, int out_fd) {
    struct fd_chan *f = calloc(1, sizeof(*f));
    if (!f) {
        return NULL;
    }
    f->base.ops = &fd_ops;
    f->base.mode = CHAN_SEND | CHAN_RECV;
    f->in_fd = in_fd;
    f->out_fd = out_fd;
    lr_init(&f->lr, in_fd);
    return &f->base;
}
