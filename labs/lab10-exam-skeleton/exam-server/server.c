/* server.c -- one listener function and six server-specific accept
 * loops.
 *
 * STUDENT WORK:
 *   TODO 2: create_listening_socket()           socket -> bind ->
 * listen
 *   TODO 3: run_iterative_server()              accept -> serve ->
 * accept
 *   TODO 4: run_thread_per_connection_server()  accept -> create
 * thread
 *   TODO 5: run_poll_server()                   accept + poll loop
 *   TODO 6: run_select_server()                 accept + select loop
 *   TODO 7: run_process_per_connection_server() accept -> fork
 *   TODO 8: run_epoll_server()                  accept + epoll loop
 *
 * GIVEN: main(), settings/cleanup, shared client arrays, and request
 * handling. exam.c handles login, questions, grading, and results.
 */
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_CLIENTS 1024
#define WAIT_TIMEOUT_MS 1000

volatile sig_atomic_t g_server_stop = 0;
static exam_database_t g_exam_database;

/* Shared storage for the SINGLE-THREADED select/poll/epoll models.
 * Each record contains the client fd (input.fd), partial input, and
 * exam-session state. Worker threads/processes own descriptors
 * directly and do not share this array. */
static connection_t g_client_array[MAX_CLIENTS];
static struct pollfd g_client_poll_fds[MAX_CLIENTS + 1];
static size_t g_client_count;

/* -------------------------------------------------------------------------
 * GIVEN: connection operations shared by every server model.
 * -------------------------------------------------------------------------
 */

void connection_init(connection_t* connection, int fd) {
    memset(connection, 0, sizeof(*connection));
    line_buffer_init(&connection->input, fd);
    exam_session_init(&connection->session);
}

ssize_t connection_read(connection_t* connection) {
    return line_buffer_receive(&connection->input);
}

int socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Own and close one blocking client for iterative/thread/process
 * models. */
void serve_blocking_client(int fd) {
    connection_t connection;
    connection_init(&connection, fd);

    for (;;) {
        ssize_t n = connection_read(&connection);
        if (n > 0) {
            if (exam_process_buffered_requests(&connection) < 0)
                break;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break; /* EOF or error */
        }
    }
    close(fd);
}

/* GIVEN for readiness models: service one readable client.
 * Return 0 to keep it, or -1 when the caller should remove and close
 * it. */
int service_ready_connection(connection_t* connection) {
    ssize_t n = connection_read(connection);
    if (n > 0) return exam_process_buffered_requests(connection);
    if (n == 0) return -1;
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;
    return -1;
}

/* -------------------------------------------------------------------------
 * GIVEN: the same client arrays and API for select, poll, and epoll.
 * No allocation, free, or mutex is needed in the student event loops.
 * -------------------------------------------------------------------------
 */

/* Take ownership of fd on BOTH success and failure. Make it
 * nonblocking and store its complete state in the next shared array
 * slot. */
int client_array_add(int fd) {
    if (g_client_count == MAX_CLIENTS ||
        socket_set_nonblocking(fd) < 0) {
        close(fd);
        return -1;
    }
    connection_init(&g_client_array[g_client_count], fd);
    g_client_count++;
    return 0;
}

connection_t* client_array_find(int fd) {
    for (size_t i = 0; i < g_client_count; i++) {
        if (g_client_array[i].input.fd == fd)
            return &g_client_array[i];
    }
    return NULL;
}

/* Remove by dense-array index. The last record fills the vacant slot.
 */
void client_array_remove_at(size_t index) {
    if (index >= g_client_count) return;
    close(g_client_array[index].input.fd);
    g_client_count--;
    if (index != g_client_count)
        g_client_array[index] = g_client_array[g_client_count];
    memset(&g_client_array[g_client_count], 0,
           sizeof(g_client_array[g_client_count]));
}

void client_array_remove_fd(int fd) {
    for (size_t i = 0; i < g_client_count; i++) {
        if (g_client_array[i].input.fd == fd) {
            client_array_remove_at(i);
            return;
        }
    }
}

static void client_array_close_all(void) {
    while (g_client_count > 0)
        client_array_remove_at(g_client_count - 1);
}

/* Build poll's listener + current-client view in the shared pollfd
 * array. */
size_t client_array_prepare_poll(int listen_fd) {
    g_client_poll_fds[0].fd = listen_fd;
    g_client_poll_fds[0].events = POLLIN;
    g_client_poll_fds[0].revents = 0;
    for (size_t i = 0; i < g_client_count; i++) {
        g_client_poll_fds[i + 1].fd = g_client_array[i].input.fd;
        g_client_poll_fds[i + 1].events = POLLIN;
        g_client_poll_fds[i + 1].revents = 0;
    }
    return g_client_count + 1;
}

/* -------------------------------------------------------------------------
 * TODO 2: CREATE THE LISTENING SOCKET -- nothing else.
 * socket(AF_INET, SOCK_STREAM, 0); set SO_REUSEADDR; zero
 * sockaddr_in; use AF_INET, htonl(INADDR_ANY), and htons(port);
 * bind(); listen(fd, 1024). Port 0 is valid: the kernel chooses a
 * free port. Return the listener on success. On failure, call
 * perror(), close any open descriptor, and return -1. Do NOT call
 * accept() here: each run_*_server() below owns its accept logic.
 * -------------------------------------------------------------------------
 */
static int create_listening_socket(int port) {
    (void)port;
    fprintf(stderr,
            "create_listening_socket: TODO 2 not implemented\n");
    return -1;
}

/* TODO 3: ITERATIVE -- your FIRST accepted server connection.
 * Until g_server_stop: accept(), retry EINTR, then
 * serve_blocking_client(fd). Only after that client finishes may you
 * accept the next one. The given serve_blocking_client() closes fd. A
 * bad client must not stop the listener. */
int run_iterative_server(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_iterative_server: TODO 3 not implemented\n");
    return -1;
}

/*TODO 4*/
/* GIVEN worker: its heap argument belongs to exactly one thread. */
void* thread_client_worker(void* argument) {
    int fd = *(int*)argument;
    free(argument);
    serve_blocking_client(fd);
    return NULL;
}

/* TODO 4: THREAD PER CONNECTION -- only the accept/dispatch policy
 * changes. Allocate a separate heap int for each accepted fd. Pass it
 * to a detached thread running thread_client_worker(); NEVER pass
 * &client_fd from the loop. On allocation/thread failure, free the
 * argument and close that client. */
int run_thread_per_connection_server(int listen_fd) {
    (void)listen_fd;
    fprintf(
        stderr,
        "run_thread_per_connection_server: TODO 4 not implemented\n");
    return -1;
}

/* TODO 5: ONE THREAD, poll().
 * count = client_array_prepare_poll(listen_fd), then
 * poll(g_client_poll_fds, count, WAIT_TIMEOUT_MS).
 * Process existing clients from last to first:
 * service_ready_connection(), then client_array_remove_at() on
 * closure. Descending order makes swap-with-last removal safe. THEN,
 * if the listener is readable, accept ONE client and pass its fd to
 * client_array_add(). Retry EINTR; ignore EAGAIN/EWOULDBLOCK.
 * clean_up() closes remaining clients. */
int run_poll_server(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_poll_server: TODO 5 not implemented\n");
    return -1;
}

/* TODO 6 (HOMEWORK): ONE THREAD, select().
 * Rebuild read_fds and a 1-second timeout before EVERY select():
 * FD_ZERO, FD_SET(listener), then FD_SET(each input.fd in
 * g_client_array). Track max_fd. Close an accepted fd >= FD_SETSIZE
 * instead of using FD_SET on it. Service and remove clients using the
 * same API and descending order as poll(). Accept AFTER servicing old
 * clients. clean_up() closes remaining clients. */
int run_select_server(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_select_server: TODO 6 not implemented\n");
    return -1;
}

/* TODO 7 (HOMEWORK): PROCESS PER CONNECTION.
 * Child: close(listen_fd), serve_blocking_client(client_fd),
 * _exit(0). Parent: close(client_fd) immediately and continue
 * accepting. SIGCHLD reaping is GIVEN in init_settings(); do not add
 * signal code here. */
int run_process_per_connection_server(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr,
            "run_process_per_connection_server: TODO 7 not "
            "implemented\n");
    return -1;
}

/* TODO 8 (HOMEWORK): ONE THREAD, Linux epoll (level-triggered).
 * Register listen_fd and clients through event.data.fd (no pointers
 * into the array). After accept, client_array_add(fd), then register
 * EPOLLIN|EPOLLRDHUP; on epoll_ctl failure call
 * client_array_remove_fd(fd). epoll_wait(...,WAIT_TIMEOUT_MS).
 * listen_fd identifies the listener. For a client event,
 * client_array_find(fd), service_ready_connection(), and
 * client_array_remove_fd(fd). close(fd) removes it from epoll. Close
 * epoll_fd on exit; clean_up() closes all clients still in the shared
 * array. */
int run_epoll_server(int listen_fd) {
    (void)listen_fd;
    fprintf(stderr, "run_epoll_server: TODO 8 not implemented\n");
    return -1;
}

/* -------------------------------------------------------------------------
 * GIVEN INITIAL SETTINGS AND CLEANUP.
 * -------------------------------------------------------------------------
 */

static int parse_int_in_range(const char* text, int min, int max,
                              int* out) {
    char* end;
    if (!text || !*text) return -1;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value < min || value > max)
        return -1;
    *out = (int)value;
    return 0;
}

/* GIVEN: install signal infrastructure and initialize the
 * exam/database. Signals are covered later, so do not modify
 * these functions. */

static void handle_stop_signal(int sig) {
    (void)sig;
    g_server_stop = 1;
}

static int init_settings(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_stop_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0 ||
        signal(SIGPIPE, SIG_IGN) == SIG_ERR ||
        signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror("signal setup");
        return -1;
    }

    if (mkdir("results", 0755) < 0 && errno != EEXIST) {
        perror("mkdir results");
        return -1;
    }
    if (exam_database_load(&g_exam_database, "data") < 0) {
        fprintf(stderr,
                "cannot load data/ -- run `make data` first\n");
        return -1;
    }
    exam_set_database(&g_exam_database);
    return 0;
}

/* GIVEN: safe on every exit path, including partial initialization.
 */
static void clean_up(int listen_fd) {
    if (listen_fd >= 0) close(listen_fd);
    client_array_close_all();
    exam_database_free(&g_exam_database);
}

static int print_listening_port(int listen_fd) {
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    if (getsockname(listen_fd, (struct sockaddr*)&address, &length) <
        0) {
        perror("getsockname");
        return -1;
    }
    printf("LISTENING %d\n", ntohs(address.sin_port));
    fflush(stdout);
    return 0;
}

static int run_selected_server(const char* mode, int listen_fd) {
    if (!strcmp(mode, "iterative"))
        return run_iterative_server(listen_fd);
    if (!strcmp(mode, "thread"))
        return run_thread_per_connection_server(listen_fd);
    if (!strcmp(mode, "fork"))
        return run_process_per_connection_server(listen_fd);
    if (!strcmp(mode, "select")) return run_select_server(listen_fd);
    if (!strcmp(mode, "poll")) return run_poll_server(listen_fd);
    if (!strcmp(mode, "epoll")) return run_epoll_server(listen_fd);
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}

/* GIVEN: main only shows the program lifecycle. socket/bind/listen
 * are in one student function; accept and client dispatch are in the
 * selected model. */
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "usage: %s iterative|thread|fork|select|poll|epoll "
                "[port]\n",
                argv[0]);
        return 2;
    }

    int port = 5200;
    if (argc == 3 &&
        parse_int_in_range(argv[2], 0, 65535, &port) < 0) {
        fprintf(stderr, "port must be an integer in 0..65535\n");
        return 2;
    }

    int listen_fd = -1;
    if (init_settings() < 0) { /* GIVEN */
        clean_up(listen_fd);   /* GIVEN */
        return 1;
    }

    listen_fd = create_listening_socket(port); /* TODO 2 */
    if (listen_fd < 0 || print_listening_port(listen_fd) < 0) {
        clean_up(listen_fd); /* GIVEN */
        return 1;
    }

    int rc = run_selected_server(argv[1], listen_fd); /* TODO 3..8 */
    clean_up(listen_fd);                              /* GIVEN */
    return rc == 0 ? 0 : 1;
}
