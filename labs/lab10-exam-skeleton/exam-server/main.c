/* main.c -- GIVEN. Listener setup, common helpers, and signal infrastructure.
 * Usage: bin/examsrv MODE [PORT]. Signals are not student work in Lab 10. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exam.h"
#include "server.h"

volatile sig_atomic_t g_stop = 0;
static void on_stop(int sig) { (void)sig; g_stop = 1; }

/* GIVEN: checked integer conversion; rejects junk, overflow, and bad ranges. */
static int parse_int(const char *text, int min, int max, int *out)
{
    char *end;
    if (!text || !*text) return -1;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value < min || value > max)
        return -1;
    *out = (int)value;
    return 0;
}

void conn_init(conn_t *c, int fd)
{
    memset(c, 0, sizeof(*c));
    lb_init(&c->lb, fd);
    session_init(&c->sess);
}

ssize_t conn_read(conn_t *c) { return lb_read(&c->lb); }

int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* GIVEN for iterative/thread/fork models: own and close one blocking client. */
void serve_client(int fd)
{
    conn_t c;
    conn_init(&c, fd);

    for (;;) {
        ssize_t n = conn_read(&c);
        if (n > 0) {
            if (conn_serve(&c) < 0) break;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;               /* EOF or error */
        }
    }
    close(fd);
}

static int make_listener(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        perror("setsockopt"); close(fd); return -1;
    }
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 1024) < 0) {
        perror("listen"); close(fd); return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "usage: %s iterative|thread|fork|select|poll|epoll [port]\n",
                argv[0]);
        return 2;
    }
    int port = 5200;
    if (argc == 3 && parse_int(argv[2], 0, 65535, &port) < 0) {
        fprintf(stderr, "port must be an integer in 0..65535\n");
        return 2;
    }

    if (mkdir("results", 0755) < 0 && errno != EEXIST) {
        perror("mkdir results");
        return 1;
    }

    static exam_db_t db;
    if (exam_db_load(&db, "data") < 0) {
        fprintf(stderr, "cannot load data/ -- run `make data` first\n");
        return 1;
    }
    exam_set_db(&db);

    /* GIVEN signal infrastructure (covered in the next lecture):
     * - no SA_RESTART: accept/poll/select/epoll_wait return EINTR on shutdown;
     * - SIGPIPE is ignored; send_all also uses MSG_NOSIGNAL;
     * - SIGCHLD auto-reaping prevents zombies in the fork model. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGTERM, &sa, NULL) < 0 ||
        signal(SIGPIPE, SIG_IGN) == SIG_ERR ||
        signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror("signal setup");
        return 1;
    }

    int lfd = make_listener(port);
    if (lfd < 0) return 1;
    struct sockaddr_in a;
    socklen_t al = sizeof(a);
    if (getsockname(lfd, (struct sockaddr *)&a, &al) < 0) {
        perror("getsockname");
        close(lfd);
        return 1;
    }
    printf("LISTENING %d\n", ntohs(a.sin_port));
    fflush(stdout);

    int rc;
    if (!strcmp(argv[1], "iterative")) rc = run_iterative(lfd);
    else if (!strcmp(argv[1], "thread")) rc = run_thread(lfd);
    else if (!strcmp(argv[1], "fork")) rc = run_fork(lfd);
    else if (!strcmp(argv[1], "select")) rc = run_select(lfd);
    else if (!strcmp(argv[1], "poll")) rc = run_poll(lfd);
    else if (!strcmp(argv[1], "epoll")) rc = run_epoll(lfd);
    else { fprintf(stderr, "unknown mode %s\n", argv[1]); rc = 2; }

    close(lfd);
    return rc == 0 ? 0 : 1;
}
