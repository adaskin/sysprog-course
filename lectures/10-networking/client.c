#include <arpa/inet.h> /*for htonl, htons, ntohl, ntohs: convert values between host and network byte order */
#include <netinet/in.h> /*for PF_INET, AF_INET, IPPPROTO_TCP and etc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /*for memset*/
#include <sys/socket.h> /*for socket(int domain, int type, int protocol);*/
#include <unistd.h>

void print_socket_info(char *msg, struct sockaddr_in *s) {
    printf("socket %s: addr:%s, port: %d\n", msg, inet_ntoa(s->sin_addr),
           ntohs(s->sin_port));
}

#define PORT_NUMBER 5100
#define SERVER_IP_ADDRESS "127.0.0.1"
int main() {
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(PORT_NUMBER);

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd == -1) {
        perror("socket");
        exit(1);
    }
    if (connect(sfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("connect");
        exit(1);
    }
    print_socket_info("server info", &srv_addr);
    struct sockaddr_in localinfo;
    socklen_t len = sizeof(localinfo);
    getsockname(sfd, &localinfo, &len);
    print_socket_info("local info", &localinfo);
    /*send recv message*/
    char msg[] = "client says hi";
    char buf[256] = {0};
    int b = send(sfd, msg, sizeof(msg), 0);
    printf("client sent %d byte msg\n", b);
    int r = recv(sfd, buf, sizeof(buf), 0);
    if (r != -1) {
        printf("client: incoming message: %.256s", buf);
    }

    close(sfd);
}