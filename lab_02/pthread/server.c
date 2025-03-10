#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>

#define MSGSIZE 256
#define WRITE_QUEUE 0
#define READ_QUEUE 1
#define ACTIVE_WRITER 2
#define ACTIVE_READERS 3
#define SHADOW_SEM 4
#define READERS_COUNT 3
#define WRITERS_COUNT 3
#define SLEEP_READ_MAX 2
#define SLEEP_WRITE_MAX 2
#define v 1
#define p -1
#define BUFFERSIZE 26
#define SERV_PORT 9876

int fl = 1;
char buf[BUFFERSIZE];

struct sembuf start_read[] = {
    {READ_QUEUE, v, 0},
    {WRITE_QUEUE, 0, 0},
    {ACTIVE_WRITER, 0, 0},
    {ACTIVE_READERS, v, 0},
    {READ_QUEUE, p, 0}
};
struct sembuf stop_read[] = {
    {ACTIVE_READERS, p, 0}
};
struct sembuf start_write[] = {
    {WRITE_QUEUE, v, 0},
    {SHADOW_SEM, v, 0},
    {ACTIVE_READERS, 0, 0},
    {ACTIVE_WRITER, v, 0},
    {WRITE_QUEUE, p, 0}
};
struct sembuf stop_write[] = {
    {SHADOW_SEM, p, 0},
    {ACTIVE_WRITER, p, 0}
};

typedef enum type {
    READ,
    WRITE
} type_t;

int semid;

void reader(int semid, int connfd)
{
    int errorcode = 0;
    int e = 0;
    e = semop(semid, start_read, 5);
    if (e == -1)
    {
        if (errno == EINTR)
        {
            printf("Thread %ld caught interrupt on block\n", pthread_self());
            exit(EXIT_FAILURE);
        }
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
    errorcode = 0;
    if (write(connfd, buf, BUFFERSIZE) < 0)
    {
        // Обработка запроса клиента (например, вызов handle_client)
        perror("write_reader");
        exit(EXIT_FAILURE);
    }
    e = semop(semid, stop_read, 1);
    if (e == -1)
    {
        perror("semop\n");
        printf("Client exit\n");
        exit(EXIT_FAILURE);
    }
}

void writer(int semid, int connfd, int index)
{
    int err = 0;
    int errorcode = 0;
    err = semop(semid, start_write, 5);
    if (err == -1)
    {
        if (errno == EINTR)
        {
            printf("Thread %ld caught interrupt on block\n", pthread_self());
            exit(EXIT_FAILURE);
        }
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
    if (index >= 0 && index < BUFFERSIZE)
    {
        if (buf[index] == ' ')
        {
            errorcode = 1;
        }
        else
        {
            buf[index] = ' ';
            printf("Occupied buf[%d]\n", index);
        }
    }
    else
    {
        errorcode = -1;
    }
    if (write(connfd, &errorcode, sizeof(errorcode)) < 0)
    {
        perror("write_writer");
        exit(EXIT_FAILURE);
    }
    err = semop(semid, stop_write, 2);
    if (err == -1)
    {
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
}

void sigint_handler(int signo)
{
    if (semctl(semid, 0, IPC_RMID) == -1)
    {
        perror("semctl");
    }
    exit(EXIT_SUCCESS);
}

void handle_client(int connfd, int semid)
{
    int n;
    enum type type;
    int arg;
    for (int i = 0; i < 2; i++)
    {
        if ((n = read(connfd, &type, sizeof(type))) != sizeof(type))
        {
            perror("read_handler");
            exit(EXIT_FAILURE);
        }
        switch (type)
        {
            case READ:
                reader(semid, connfd);
                break;
            case WRITE:
                if (read(connfd, &arg, sizeof(arg)) < 0)
                {
                    perror("read_handler");
                    exit(EXIT_FAILURE);
                }
                writer(semid, connfd, arg);
                break;
            default:
                printf("Invalid request type %d\n", type);
                break;
        }
    }
}

// void *client_handler(void *arg)
    // Обработка запроса клиента (например, вызов handle_client)
// {
//     int connfd = *(int *)arg;
//     free(arg);
//     handle_client(connfd, semid);
//     close(connfd);
//     pthread_exit(NULL);
// }
typedef struct {
    int connfd;
    int cpuID;
} thread_data_t;

void *client_handler(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    int connfd = data->connfd;
    int cpuID = data->cpuID;
    free(data);
    int num = sysconf(_SC_NPROCESSORS_CONF);
    if (cpuID < 0 || cpuID >= num) {
        fprintf(stderr, "cpuID %d is out of range (0 - %d). Using 0 instead.\n", cpuID, num - 1);
        cpuID = 0;
    }
    cpu_set_t mask;
    cpu_set_t get;
    CPU_ZERO(&mask);
    //CPU_SET(cpuID, &mask);
    // int s = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    // if (s != 0)
    //     fprintf(stderr, "pthread_setaffinity_np error: %s\n", strerror(s));
    CPU_ZERO(&get);
    int s = pthread_getaffinity_np(pthread_self(), sizeof(get), &get);
    if (s != 0)
        fprintf(stderr, "pthread_getaffinity_np error: %s\n", strerror(s));
    printf("Thread %lu is serving client ", (unsigned long)pthread_self());
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    if (getpeername(connfd, (struct sockaddr *)&peer, &len) != -1)
        printf("%d", ntohs(peer.sin_port));
    else
        perror("getpeername");
    printf(" on CPU(s): ");
    for (int i = 0; i < num; i++)
        if (CPU_ISSET(i, &get))
            printf("%d ", i);
    int cpu = sched_getcpu();
    printf("Thread %lu is running on CPU: %d", (unsigned long)pthread_self(), cpu);
    printf("\n");
    handle_client(connfd, semid);
    close(connfd);
    pthread_exit(NULL);
}

int main(void)
{
    int listenfd, connfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    pthread_attr_t attr;
    pthread_t tid;
    key_t semkey;
    struct sembuf sem_op;
    semkey = ftok("./key", 1);
    if (semkey == -1)
    {
        perror("semkey");
        exit(EXIT_FAILURE);
    }
    semid = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    if (semid == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < BUFFERSIZE; i++)
        buf[i] = 'a' + i;
    if (semctl(semid, ACTIVE_READERS, SETVAL, 0) == -1)
    {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, ACTIVE_WRITER, SETVAL, 0) == -1)
    {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, WRITE_QUEUE, SETVAL, 0) == -1)
    {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, READ_QUEUE, SETVAL, 0) == -1)
    {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, SHADOW_SEM, SETVAL, 1) == -1)
    {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, sigint_handler);
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(listenfd, 5) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    int num = sysconf(_SC_NPROCESSORS_CONF);
    int i = 0;
    while (1)
    {
        if (buf[BUFFERSIZE - 1] == ' ')
        {
            printf("Buffer is full\n");
            break;    
        }
        clilen = sizeof(cliaddr);
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) == -1)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // int *client_sock = malloc(sizeof(int));
        // *client_sock = connfd;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        thread_data_t *data = malloc(sizeof(thread_data_t));
        if (!data) {
            perror("malloc");
            close(connfd);
            continue;
        }
        data->connfd = connfd;
        data->cpuID = 0;
        if (pthread_create(&tid, &attr, client_handler, data) != 0) {
            perror("pthread_create");
            free(data);
            close(connfd);
            continue;
        }
        //i = (i+1)%num;
    }
    pthread_attr_destroy(&attr);
    return 0;
}
