#include <arpa/inet.h> /*htonl, htons, ntohl, ntohs: convert values between host and network byte order */
#include <netinet/in.h> /*for PF_INET, AF_INET, IPPROTO_TCP and etc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> /*for socket(int domain, int type, int protocol);*/
#include <unistd.h>

void print_socket_info(char *msg, struct sockaddr_in *s) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip)); /* thread-safe, unlike inet_ntoa */
    printf("socket %s: addr:%s, port: %d\n", msg, ip, ntohs(s->sin_port));
}

#define NUMBER_OF_CONNECTIONS 10
#define PORT_NUMBER 5100
int main(void) {
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT_NUMBER);

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd == -1) {
        perror("socket");
        exit(1);
    }
    int opt = 1; /* allow quick restart: avoids "Address already in use" */
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        perror("bind");
        exit(1);
    }
    if (listen(sfd, NUMBER_OF_CONNECTIONS) == -1) {
        perror("listen");
        exit(1);
    }
    print_socket_info("me server", &my_addr);

    struct sockaddr_in accepted_addr;
    memset(&accepted_addr, 0, sizeof(accepted_addr));
    socklen_t size = sizeof(accepted_addr);
    /* TODO (students): this server handles ONE client at a time. While it
     * talks to a client (the recv loop below), every other client waits in the
     * listen() queue. Convert it so that clients are served concurrently:
     *   (a) multiprocess: fork() a child per accepted connection; the child
     *       closes sfd and serves the client, the parent closes sfd_accepted.
     *       Avoid zombies (signal(SIGCHLD, SIG_IGN) or waitpid with WNOHANG).
     *   (b) multithread: pthread_create() a (detached) thread per connection.
     *       Give each thread its OWN copy of the fd (do not pass the address
     *       of a loop variable: data race!).
     * Test with several clients at once, e.g. `nc 127.0.0.1 5100` in 3 terminals. */
    for (;;) {
        size = sizeof(accepted_addr); /* accept() overwrites it: reset every time */
        int sfd_accepted =
            accept(sfd, (struct sockaddr *)&accepted_addr, &size);
        if (sfd_accepted == -1) {
            perror("accept");
            exit(1);
        }
        print_socket_info("accepted client", &accepted_addr);

        /* NOTE: TCP is a byte STREAM, not a message protocol. One send() on the
         * client does not guarantee one recv() here: a "message" can arrive split
         * in pieces or glued to the next one. Real protocols need framing
         * (a delimiter or a length prefix, see lecture 10b). This loop simply
         * reads until the client closes the connection. */
        char buf[256];
        ssize_t r;
        while ((r = recv(sfd_accepted, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[r] = '\0'; /* recv does not NUL-terminate */
            printf("server: received %zd bytes: %s\n", r, buf);
            char msg[] = "server says hi";
            ssize_t b = send(sfd_accepted, msg, sizeof(msg), 0);
            printf("server sent %zd byte msg\n", b);
        }
        if (r == -1) perror("recv"); /* r == 0: the client closed normally */

        close(sfd_accepted);
    }
    close(sfd);
}
