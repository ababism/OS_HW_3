#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
int client_sock;

void sig_handler(int signum) {
    close(client_sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <ip> <port> <client_type>\n", argv[0]);
        return 1;
    }
    char *cl_type = argv[3];
    signal(SIGINT, sig_handler);
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Создание сокета
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    // Подключение к серверу
    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection error");
        return 1;
    }
    int type_code;

    if (strcmp("bear", cl_type) == 0) {
        type_code = 1;
    } else {
        type_code = 0;
    }

    if (send(client_sock, &type_code, sizeof(int), 0) < 0) {
        perror("Send error");
        close(client_sock);
        exit(1);
    }
    if (type_code == 1) {
        // bear
        for (;;) {
            int sips;
            if (recv(client_sock, &sips, sizeof(int), 0) <= 0) {
                perror("Receive error");
                break;
            }
            printf("Bear (you) woke up \n");
            printf("Enter any number to eat honey: ");
            int input;
            scanf("%d", &input);
            if (send(client_sock, &input, sizeof(int), 0) < 0) {
                perror("Send error");
                break;
            }
            printf("Bear ate %d sips of honey\n", sips);
            printf("Bear went to sleep. Zzz...\n");
        }
    } else {
        // bee
        while (1) {
            int honey;
            int ans;
            printf("Enter how many honey to add to pot: ");
            scanf("%d", &honey);

            // Отправка команды серверу
            if (send(client_sock, &honey, sizeof(int), 0) < 0) {
                perror("Send error");
                break;
            }
            if (recv(client_sock, &ans, sizeof(int), 0) <= 0) {
                perror("Receive error");
                break;
            }
            if (ans == 2) {
                printf("Bee is waking up the bear\n");
            } else if (ans == 1) {
                printf("Bee added honey\n");
            } else {
                printf("Can't add honey (pot is full)\n");
            }
        }
    }

    close(client_sock);
    return 0;
}