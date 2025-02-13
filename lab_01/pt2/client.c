#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKETNAME "socket.s"

int main()
{
    struct sockaddr_un addr;
    int client_socket;
    char buffer[128];
    pid_t pid = getpid();
    client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (client_socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKETNAME, sizeof(addr.sun_path) - 1);
    snprintf(buffer, sizeof(buffer), "%d", pid);
    if (sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1)
    {
        fprintf(stderr, "server shut\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Sent message: %s\n", buffer);
    close(client_socket);
    return 0;
}