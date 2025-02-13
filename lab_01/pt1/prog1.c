#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define MESSAGE_SIZE 256

int main()
{
    int fdsock[2];
    pid_t pid;
    char buffer[MESSAGE_SIZE];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdsock) == -1)
    {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        close(fdsock[0]);  // Закрываем один из сокетов в потомке
        // Получаем сообщение от предка
        if (read(fdsock[1], buffer, MESSAGE_SIZE) == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("Child received: %s\n", buffer);
        // Отправляем сообщение "how are you" предку
        strcpy(buffer, "how are you");
        printf("Child sent: %s\n", buffer);
        if (write(fdsock[1], buffer, MESSAGE_SIZE) == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        // Получаем сообщение "good" от предка
        if (read(fdsock[1], buffer, MESSAGE_SIZE) == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("Child received: %s\n", buffer);
        // Отправляем сообщение "goodbye" предку
        strcpy(buffer, "goodbye");
        printf("Child sent: %s\n", buffer);
        if (write(fdsock[1], buffer, MESSAGE_SIZE) == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        close(fdsock[1]);
        exit(EXIT_SUCCESS);
    }
    else
    {
        // Код процесса-предка
        close(fdsock[1]);  // Закрываем один из сокетов в предке
        // Отправляем сообщение "hello" потомку
        strcpy(buffer, "hello");
        printf("Parent sent: %s\n", buffer);
        if (write(fdsock[0], buffer, MESSAGE_SIZE) == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        // Получаем сообщение "how are you" от потомка
        if (read(fdsock[0], buffer, MESSAGE_SIZE) == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("Parent received: %s\n", buffer);
        // Отправляем сообщение "good" потомку
        strcpy(buffer, "good");
        printf("Parent sent: %s\n", buffer);
        if (write(fdsock[0], buffer, MESSAGE_SIZE) == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        // Получаем сообщение "goodbye" от потомка
        if (read(fdsock[0], buffer, MESSAGE_SIZE) == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("Parent received: %s\n", buffer);
        close(fdsock[0]);  // Закрываем сокет в предке
        wait(NULL);  // Ждем завершения потомка
        exit(EXIT_SUCCESS);
    }

    return 0;
}
