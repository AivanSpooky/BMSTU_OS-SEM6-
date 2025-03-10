#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFERSIZE 26
#define SERV_PORT 9877
#define SERV_IP "127.0.0.1"
//"172.20.68.35" 
//"127.0.0.1"

typedef enum type
{
    READ,
    WRITE
} type_t;
int clntsock;
char buffer[BUFFERSIZE];
void sigint_handler(int signo)
{
    close(clntsock);
    printf("Client terminated\n");
    exit(EXIT_SUCCESS);
}
int find_free_index(char *buf, int size)
{
    for (int i = 0; i < size; i++)
        if (buf[i] != ' ')
            return i;
    return -1;
}
int sock_create(void)
{
    int clntsock;
    if ((clntsock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return clntsock;
}
void sock_connect(struct sockaddr_in *servaddr)
{
    if (connect(clntsock, (struct sockaddr *)servaddr, sizeof(*servaddr)) == -1)
    {
        printf("Server is closed");
        exit(EXIT_FAILURE);
    } 
}
int main(void)
{
    struct sockaddr_in servaddr;
    int index;
    int errorcode = 0;
    enum type type;
    signal(SIGINT, sigint_handler);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(SERV_PORT);
    if (inet_pton(AF_INET, SERV_IP, &servaddr.sin_addr) <= 0)
    {
        perror("Неверный IP-адрес или ошибка inet_pton");
        exit(EXIT_FAILURE);
    }
    srand(time(NULL));
    while (1)
    {
        clntsock = sock_create();
        type = READ;
        sock_connect(&servaddr);
        if (write(clntsock, &type, sizeof(type)) < 0)
        {
            perror("write_reader");
            exit(EXIT_FAILURE);
        }
        if (read(clntsock, buffer, sizeof(buffer)) < 0)
        {
            perror("read_writer");
            exit(EXIT_FAILURE);
        }
        sleep(rand() % 3 + 1);
        index = find_free_index(buffer, BUFFERSIZE);
        printf("received: [");
        for (int i = 0; i < index; i++)
        {
            printf("*");
        }
        for (int i = index; i < BUFFERSIZE; i++)
        {
            printf("%c", buffer[i]);
        }
        printf("]\n");
        if (index == -1)
        {
            printf("No free index available\n");
            break;
        }
        type = WRITE;
        index = index;
        printf("send to index %d\n", index);
        if (write(clntsock, &type, sizeof(type)) < 0)
        {
            perror("write_writer");
            exit(EXIT_FAILURE);
        }
        if (write(clntsock, &index, sizeof(index)) < 0)
        {
            perror("write_writer");
            exit(EXIT_FAILURE);
        }
        if (read(clntsock, &errorcode, sizeof(errorcode)) < 0)
        {
            perror("read_writer");
            exit(EXIT_FAILURE);
        }
        if (errorcode == 1)
        {
            printf("char on index %d is occupied\n", index);
        }
        errorcode = 0;

        close(clntsock);
        
        sleep(rand() % 3 + 1);
    }
    return 0;
}
