#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

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
#define SERV_PORT 9877
int fl = 1;
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
typedef enum type
{
    READ,
    WRITE
} type_t;
int shmid;
int semid;
char *buf;
void reader(key_t semid, int connfd)
{
    int err = 0;
    err = semop(semid, start_read, 5);
    if (err == -1)
    {
        if (errno == EINTR)
        {
            printf("PID %d caught interrupt on block\n", getpid());
            exit(1);
        }
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
    strncpy(buf, buf, BUFFERSIZE);
    if (write(connfd, buf, BUFFERSIZE) < 0)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
    err = semop(semid, stop_read, 1);
    if (err == -1)
    {
        perror("semop\n");
        printf("Error at read 2!\n");
        printf("Client exit\n");
        exit(EXIT_FAILURE);
    }
}
void writer(key_t semid, int connfd, int index)
{
    int err = 0;
    int e = 0;
    err = semop(semid, start_write, 5);
    if (err == -1)
    {
        if (errno == EINTR)
        {
            printf("PID %d caught interrupt on block\n", getpid());
            exit(EXIT_FAILURE);
        }
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
    if (index >= 0 && index < BUFFERSIZE)
    {
        if (buf[index] == ' ')
            e = 1;
        else
        {
            buf[index] = ' ';
            printf("Occupied [%d]\n", index);
        }
    }
    else
        e = -1;
    if (write(connfd, &e, sizeof(e)) < 0)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
    err = semop(semid, stop_write, 2);
    if (err == -1)
    {
        perror("semop\n");
        exit(EXIT_FAILURE);
    }
}

void sigint_h(int signo)
{
    if (shmdt(buf) == -1)
        perror("shmdt");
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        perror("shmctl");
    if (semctl(semid, 0, IPC_RMID) == -1)
        perror("semctl");
    exit(EXIT_SUCCESS);
}

void sigchld_h(int signo)
{
    pid_t childpid;
    int stat;
    while ((childpid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("Child %d exited with status %d\n", childpid, stat);
}

void request_type(int connfd, key_t semid)
{
    int n;
    enum type type;
    int arg;
    for (int i = 0; i < 2; i++)
    {
        if ((n = read(connfd, &type, sizeof(type))) != sizeof(type))
        {
            perror("request_type");
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
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                writer(semid, connfd, arg);
                break;
            default:
                break;
        }
    }
}
int main(void)
{
    int listenfd, connfd;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    key_t shmkey, semkey;
    struct sembuf sem_op;
    shmkey = ftok("shm", 'M');
    if (shmkey == -1) {
        perror("ftok for shared memory");
        exit(EXIT_FAILURE);
    }
    semkey = ftok("./key", 1);
    if (semkey == -1)
    {
        perror("semkey");
        exit(EXIT_FAILURE);
    }
    shmid = shmget(shmkey, getpagesize(), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    buf = (char *)shmat(shmid, NULL, 0);
    if (buf == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    semid = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    if (semid == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    *buf = 'a';
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

    for (int i = 0; i < BUFFERSIZE; i++)
        buf[i] = 'a' + i;
    signal(SIGINT, sigint_h);
    signal(SIGCHLD, sigchld_h);
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("SO_REUSEADDR");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
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
    while (1)
    {
        clilen = sizeof(cliaddr);
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) == -1)
        {
                perror("accept");
                exit(EXIT_FAILURE);
        }
        if ((childpid = fork()) == 0)
        {
            close(listenfd);
            request_type(connfd, semid);
            //sleep(15);
            close(connfd);
            exit(EXIT_SUCCESS);
        }
        close(connfd);
        if (buf[BUFFERSIZE - 1] == ' ')
        {
            printf("buf empty\n");
            exit(EXIT_SUCCESS);
        }
    }
    return 0;
}
