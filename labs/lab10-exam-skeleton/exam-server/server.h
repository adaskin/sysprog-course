/* server.h -- shared declarations for server.c and the given exam
 * service. */
#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <sys/types.h>

#include "exam.h"
#include "linebuf.h"

typedef struct {
    line_buffer_t input; /* input.fd plus bytes retained across recv() calls */
    exam_session_t session;
} connection_t;

extern volatile sig_atomic_t  g_server_stop; /* set by the GIVEN signal handler */

/* GIVEN connection helpers in server.c. */
void connection_init(connection_t* connection, int fd);
ssize_t connection_read(connection_t* connection);
int socket_set_nonblocking(int fd);
void serve_blocking_client(
    int fd); /* read loop + close for blocking workers */
int service_ready_connection(connection_t* connection);
void* thread_client_worker(void* argument);

/* GIVEN shared-array API for the single-threaded select/poll/epoll
 * models. client_array_add() owns and closes fd on both success and
 * failure. */
int client_array_add(int fd);
connection_t* client_array_find(int fd);
void client_array_remove_at(size_t index);
void client_array_remove_fd(int fd);
size_t client_array_prepare_poll(int listen_fd);

/* GIVEN in exam.c: handle every complete request currently buffered.
 */
int exam_process_buffered_requests(connection_t* connection);

/* Student server models in server.c. The listener is already bound.
 */
int run_iterative_server(int listen_fd);              /* TODO 3 */
int run_thread_per_connection_server(int listen_fd);  /* TODO 4 */
int run_poll_server(int listen_fd);                   /* TODO 5 */
int run_select_server(int listen_fd);                 /* TODO 6 */
int run_process_per_connection_server(int listen_fd); /* TODO 7 */
int run_epoll_server(int listen_fd);                  /* TODO 8 */

#endif
