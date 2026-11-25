/* main.c -- GIVEN. Usage: ./examsrv MODE [PORT]
 * MODE = thread | fork | select | poll | epoll.  PORT 0 = let the kernel choose.
 * Prints "LISTENING <port>" on stdout when ready (the test scripts wait for it). */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "conn.h"
#include "exam.h"
#include "srv.h"

volatile sig_atomic_t g_stop = 0;

static void on_stop(int sig) { (void)sig; g_stop = 1; }

void conn_init(conn_t *c, int fd) {
    memset(c, 0, sizeof(*c));
    c->fd = fd;
    session_init(&c->sess);
}

ssize_t conn_read(conn_t *c) {
    if (c->in_len >= sizeof(c->in)) { errno = ENOBUFS; return -1; }
    ssize_t n = recv(c->fd, c->in + c->in_len, sizeof(c->in) - c->in_len, 0);
    if (n > 0) c->in_len += (size_t)n;
    return n;
}

static int make_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 1024) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s thread|fork|select|poll|epoll [port]\n", argv[0]);
        return 2;
    }
    int port = argc > 2 ? atoi(argv[2]) : 5200;

    static exam_db_t db;
    if (exam_db_load(&db, "data") < 0) {
        fprintf(stderr, "cannot load data/ -- run `make data` first\n");
        return 1;
    }
    exam_set_db(&db);

    /* no SA_RESTART: a blocking accept()/poll()/select() must return EINTR on Ctrl-C */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int lfd = make_listener(port);
    if (lfd < 0) return 1;
    struct sockaddr_in a;
    socklen_t al = sizeof(a);
    getsockname(lfd, (struct sockaddr *)&a, &al);
    printf("LISTENING %d\n", ntohs(a.sin_port));
    fflush(stdout);

    int rc;
    if (!strcmp(argv[1], "thread"))      rc = run_thread(lfd);
    else if (!strcmp(argv[1], "fork"))   rc = run_fork(lfd);
    else if (!strcmp(argv[1], "select")) rc = run_select(lfd);
    else if (!strcmp(argv[1], "poll"))   rc = run_poll(lfd);
    else if (!strcmp(argv[1], "epoll"))  rc = run_epoll(lfd);
    else { fprintf(stderr, "unknown mode %s\n", argv[1]); rc = 2; }

    close(lfd);
    return rc == 0 ? 0 : 1;
}
