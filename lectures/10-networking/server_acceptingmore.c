#include <arpa/inet.h> /*htonl, htons, ntohl, ntohs: convert values between host and network byte order */
#include <netinet/in.h> /*for PF_INET, AF_INET, IPPPROTO_TCP and etc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> /*for socket(int domain, int type, int protocol);*/
#include <unistd.h>

void print_socket_info(char *msg, struct sockaddr_in *s) {
    printf("socket %s: addr:%s, port: %d\n", msg, inet_ntoa(s->sin_addr),
           ntohs(s->sin_port));
}

#define NUMBER_OF_CONNECTIONS 10
#define PORT_NUMBER 5100
int main() {
    struct sockaddr_in my_addr;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT_NUMBER);

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd == -1) {
        perror("socket");
        exit(1);
    }
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
    for (;;) {
        int sfd_accepted =
            accept(sfd, (struct sockaddr *)&accepted_addr, &size);
        if (sfd_accepted == -1) {
            perror("accept");
            exit(1);
        }
        print_socket_info("accepted client", &accepted_addr);

        char buf[256] = {0};
        /*recv send message*/
        int r = recv(sfd_accepted, &buf[0], sizeof(buf), 0);
        if (r != -1) {
            printf("server: incoming message: %.256s", buf);
            char msg[] = "server says hi";
            int b = send(sfd_accepted, msg, sizeof(msg), 0);
            printf("server sent %d byte msg\n", b);
        }

        close(sfd_accepted);
    }
    close(sfd);
}
