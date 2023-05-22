#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 12
#define BUFF_SIZE 1024

int pot_honey = 0;
int limit_pot = 0;

pthread_mutex_t pot_mutex;

int add_honey(int sips) {
    int res = 0;
    pthread_mutex_lock(&pot_mutex);
    if (pot_honey < limit_pot && pot_honey >= limit_pot - sips) {
        pot_honey = limit_pot;
        printf("Bee added honey. Now pot is full: %d, and bee is waking up the bear\n", pot_honey);
        res = 2;
    } else if (pot_honey < limit_pot) {
        pot_honey += sips;
        printf("Bee added honey. Now in pot: %d\n", pot_honey);
        res = 1;
    }
    pthread_mutex_unlock(&pot_mutex);
    return res;
}

void *handle_client(void *arg) {
    int client_sock = *(int *) arg;
    char buffer[BUFF_SIZE];
    /* 0 = bee; 1 = bear; */
    int client_type;
    if (recv(client_sock, &client_type, sizeof(int), 0) <= 0) {
        perror("Receive error");
        close(client_sock);
        free(arg);
        exit(1);
    }

    if (client_type == 0) {
        printf("Bee connected\n");
        for (;;) {
            // bee
            memset(buffer, 0, BUFF_SIZE);
            int added_honey;
            // Получение данных от клиента
            if (recv(client_sock, &added_honey, sizeof(int), 0) <= 0) {
                perror("Receive error");
                break;
            }
            // Обработка полученных данных
            int bee_ans = add_honey(added_honey);
            if (send(client_sock, &bee_ans, sizeof(int), 0) < 0) {
                perror("Send error");
                break;
            }
        }
        printf("Bee left\n");
    } else if (client_type == 1) {
        // bear
        printf("Bear connected\n");
        for (;;) {
            pthread_mutex_lock(&pot_mutex);
            if (pot_honey >= limit_pot) {
                pthread_mutex_unlock(&pot_mutex);
                if (send(client_sock, &limit_pot, sizeof(int), 0) < 0) {
                    perror("Send error");
                    break;
                }
                int bear_resp;
                if (recv(client_sock, &bear_resp, sizeof(int), 0) <= 0) {
                    perror("Receive error");
                    break;
                }
                pthread_mutex_lock(&pot_mutex);
                printf("Bear woke up. Ate all honey now in pot\n");
                pot_honey = 0;
                pthread_mutex_unlock(&pot_mutex);
            } else {
                pthread_mutex_unlock(&pot_mutex);
            }
            sleep(1);
        }
        printf("Bear left\n");
    } else {
        // observer
        printf("Observer connected\n");
        int prev_pot = pot_honey;
        int curr_pot = pot_honey;
        for (;;) {
            pthread_mutex_lock(&pot_mutex);
            if (curr_pot != pot_honey) {
                prev_pot = curr_pot;
                curr_pot = pot_honey;
                pthread_mutex_unlock(&pot_mutex);
                if (curr_pot >= limit_pot) {
                    sprintf(buffer, "Pot is full %d. Bee is waking up bear", limit_pot);
                    if (send(client_sock, &buffer, BUFF_SIZE, 0) < 0) {
                        perror("Send error");
                        break;
                    }
                } else if (curr_pot == 0) {
                    sprintf(buffer, "Bear ate %d sips of honey. Pot is empty.", limit_pot);
                    if (send(client_sock, &buffer, BUFF_SIZE, 0) < 0) {
                        perror("Send error");
                        break;
                    }
                } else if (curr_pot - prev_pot > 0) {
                    sprintf(buffer, "Bee added %d sips of honey. Now: %d", curr_pot - prev_pot, curr_pot);
                    if (send(client_sock, &buffer, BUFF_SIZE, 0) < 0) {
                        perror("Send error");
                        break;
                    }
                }
                int ping;
                if (recv(client_sock, &ping, sizeof(int), 0) <= 0) {
                    perror("Receive error");
                    break;
                }
                if (ping < 0) {
                    break;
                }
            } else {
                pthread_mutex_unlock(&pot_mutex);
            }
            usleep(100);
        }
        printf("Observer detached\n");
    }
    close(client_sock);
    free(arg);
    pthread_exit(NULL);
}

int server_sock, client_sock;

void sig_handler(int signum) {
    pthread_mutex_destroy(&pot_mutex);
    close(server_sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <ip> <port> <pot_limit>\n", argv[0]);
        return 1;
    }
    signal(SIGINT, sig_handler);
    char *ipAdr;
    int port;

    ipAdr = argv[1];
    port = atoi(argv[2]);
    limit_pot = atoi(argv[3]);


    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    pthread_t tid;
    pthread_mutex_init(&pot_mutex, NULL);

    // Создание сокета
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }


    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ipAdr);
    server_addr.sin_port = htons(port);

    // Привязка сокета к заданному адресу и порту
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Binding error");
        return 1;
    }

    // Прослушивание входящих соединений
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listening error");
        return 1;
    }

    printf("Server started. Waiting for clients...\n");

    for (;;) {
        // Принятие входящего соединения
        client_addr_len = sizeof(client_addr);
        if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
            perror("Accept error");
            return 1;
        }

        printf("New client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        // Создание отдельного потока для обработки клиента
        int *client_sock_ptr = (int *) malloc(sizeof(int));
        *client_sock_ptr = client_sock;

        if (pthread_create(&tid, NULL, handle_client, client_sock_ptr) != 0) {
            perror("Thread creation error");
            return 1;
        }

        // Освобождение ресурсов потока после завершения
        pthread_detach(tid);
    }

    pthread_mutex_destroy(&pot_mutex);
    close(server_sock);
    return 0;
}