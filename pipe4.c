#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 8
#define MSG_SIZE 256
#define QUEUE_NAME_LEN 64

// Struktura klienta
typedef struct {
    char name[QUEUE_NAME_LEN];
    mqd_t queue;
} Client;

// Struktura wiadomości
typedef struct {
    char sender[QUEUE_NAME_LEN];
    char text[MSG_SIZE];
} Message;

Client clients[MAX_CLIENTS];
int client_count = 0;
mqd_t server_queue;
char server_queue_name[QUEUE_NAME_LEN];

void handle_sigint(int sig);
void register_notification();
void handle_message(union sigval data);

void send_to_all_clients(const char *sender, const char *msg) {
    Message message;
    snprintf(message.sender, QUEUE_NAME_LEN, "%s", sender);
    snprintf(message.text, MSG_SIZE, "%s", msg);

    for (int i = 0; i < client_count; ++i) {
        mq_send(clients[i].queue, (char*)&message, sizeof(Message), 2);
    }
}

void register_notification() {
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = handle_message;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = &server_queue;
    
    if (mq_notify(server_queue, &sev) == -1) {
        perror("mq_notify");
        exit(EXIT_FAILURE);
    }
}

void handle_message(union sigval data) {
    mqd_t *queue = (mqd_t*)data.sival_ptr;
    Message message;
    unsigned int priority;

    register_notification();
    
    while (mq_receive(*queue, (char*)&message, sizeof(Message), &priority) != -1) {
        if (priority == 0) { // Nowy klient
            if (client_count < MAX_CLIENTS) {
                strcpy(clients[client_count].name, message.sender);
                char client_queue_name[QUEUE_NAME_LEN];
                snprintf(client_queue_name, QUEUE_NAME_LEN, "/chat_%s", message.sender);
                clients[client_count].queue = mq_open(client_queue_name, O_WRONLY);
                if (clients[client_count].queue == -1) {
                    perror("mq_open client");
                    continue;
                }
                printf("Client %s has connected!\n", message.sender);
                client_count++;
            }
        } else if (priority == 1) { // Klient się rozłączył
            for (int i = 0; i < client_count; i++) {
                if (strcmp(clients[i].name, message.sender) == 0) {
                    mq_close(clients[i].queue);
                    printf("Client %s disconnected!\n", clients[i].name);
                    clients[i] = clients[--client_count];
                    break;
                }
            }
        } else if (priority == 2) { // Wiadomość tekstowa
            printf("[%s] %s\n", message.sender, message.text);
            send_to_all_clients(message.sender, message.text);
        }
    }
    if (errno != EAGAIN) perror("mq_receive");
}

void handle_sigint(int sig) {
    send_to_all_clients("SERVER", "Server closed the connection");
    for (int i = 0; i < client_count; i++) {
        mq_close(clients[i].queue);
    }
    mq_close(server_queue);
    mq_unlink(server_queue_name);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    snprintf(server_queue_name, QUEUE_NAME_LEN, "/chat_%s", argv[1]);
    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = sizeof(Message) };
    server_queue = mq_open(server_queue_name, O_RDONLY | O_CREAT | O_NONBLOCK, 0600, &attr);
    if (server_queue == -1) {
        perror("mq_open server");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_sigint);
    register_notification();
    printf("Server started: %s\n", argv[1]);
    while (1) {
        char input[MSG_SIZE];
        if (fgets(input, MSG_SIZE, stdin) != NULL) {
            input[strcspn(input, "\n")] = 0;
            send_to_all_clients("SERVER", input);
        }
    }
    return 0;
}
