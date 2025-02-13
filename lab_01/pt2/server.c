#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKETNAME "socket.s"

int server_socket;

void handler(int sig)
{
    printf("\nRECV SIG %d\n", sig);
    close(server_socket);
    unlink(SOCKETNAME);
    exit(0);
}

int main()
{
    struct sockaddr_un addr;
    char buffer[128];
    ssize_t n;
    server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server_socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKETNAME, sizeof(addr.sun_path) - 1);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
    {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    signal(SIGALRM, handler);

    printf("running...\n");

    alarm(15);
    while(1)
    {
        n = recvfrom(server_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n == -1)
        {
            perror("recvfrom");
            continue;
        }
        buffer[n] = '\0';
        printf("Received message: %s\n", buffer);
    }
    alarm(0);

    close(server_socket);
    unlink(SOCKETNAME);
    return 0;
}
