/* example drone_client.c 
* This is a simple drone client that connects to a server and sends its status
* while receiving navigation commands.
* You should separate the client implementation from server.
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {.sin_family = AF_INET,
                                      .sin_port = htons(8080),
                                      .sin_addr.s_addr = inet_addr("127.0.0.1")};
    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    while (1) {
        // Send status update
        DroneStatus status = {.x = current_x, .y = current_y, .idle = 1};
        send(sock, &status, sizeof(status), 0);
        
        // Receive mission
        Coord target;
        recv(sock, &target, sizeof(target), 0);
        navigate_to_target(target);
        
        sleep(1);
    }
}