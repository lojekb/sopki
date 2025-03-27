#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MSG_SIZE 256
#define QUEUE_NAME_SIZE 64
#define MAX_CLIENTS 8

#define MSG_CONNECT 0
#define MSG_DISCONNECT 1
#define MSG_TEXT 2

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), exit(EXIT_FAILURE))

typedef struct {
    char name[QUEUE_NAME_SIZE];
    mqd_t queue;
} ClientData;

mqd_t server_queue;
ClientData clients[MAX_CLIENTS];
int client_count = 0;

void register_notification(mqd_t queue);
void handle_messages(union sigval sv);
void send_message(mqd_t queue, const char *message, unsigned int prio);

void register_notification(mqd_t queue) {
    struct sigevent sev = {};
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = handle_messages;
    sev.sigev_value.sival_ptr = &queue;

    if (mq_notify(queue, &sev) == -1) {
        ERR("mq_notify");
    }
}

void handle_messages(union sigval sv) {
    mqd_t *queue = (mqd_t *)sv.sival_ptr;
    char message[MSG_SIZE];
    unsigned int prio;

    while (mq_receive(*queue, message, MSG_SIZE, &prio) != -1) {
        if (prio == MSG_TEXT) {
            printf("%s\n", message);
        } else if (prio == MSG_DISCONNECT) {
            printf("Server closed the connection.\n");
            mq_close(*queue);
            exit(EXIT_SUCCESS);
        }
    }
}

void send_message(mqd_t queue, const char *message, unsigned int prio) {
    if (mq_send(queue, message, strlen(message) + 1, prio) == -1) {
        ERR("mq_send");
    }
}

void server_function(char *server_name) {
    // Open the server queue
    char server_queue_name[QUEUE_NAME_SIZE];
    snprintf(server_queue_name, QUEUE_NAME_SIZE, "/chat_%s", server_name);
    server_queue = mq_open(server_queue_name, O_RDONLY | O_CREAT, 0600, NULL);
    if (server_queue == (mqd_t)-1) {
        ERR("mq_open server");
    }

    // Handle incoming messages
    char message[MSG_SIZE];
    unsigned int prio;

    while (1) {
        if (mq_receive(server_queue, message, MSG_SIZE, &prio) != -1) {
            if (prio == MSG_CONNECT) {
                // New client connection
                if (client_count < MAX_CLIENTS) {
                    snprintf(clients[client_count].name, QUEUE_NAME_SIZE, "%s", message);
                    char client_queue_name[QUEUE_NAME_SIZE];
                    snprintf(client_queue_name, QUEUE_NAME_SIZE, "/chat_%s", message);

                    clients[client_count].queue = mq_open(client_queue_name, O_WRONLY);
                    if (clients[client_count].queue == (mqd_t)-1) {
                        ERR("mq_open client");
                    }

                    printf("Client %s has connected!\n", message);
                    send_message(clients[client_count].queue, "Welcome to the chat!", MSG_TEXT);
                    client_count++;
                }
            } else if (prio == MSG_DISCONNECT) {
                // Handle disconnection
                printf("A client disconnected\n");
            } else if (prio == MSG_TEXT) {
                // Broadcast the message to all clients
                for (int i = 0; i < client_count; ++i) {
                    char formatted_msg[MSG_SIZE];
                    snprintf(formatted_msg, MSG_SIZE, "[%s] %s", message, message);
                    send_message(clients[i].queue, formatted_msg, MSG_TEXT);
                }
            }
        } else {
            if (errno != EAGAIN) {
                ERR("mq_receive");
            }
        }
    }

    // Cleanup and close
    mq_close(server_queue);
    mq_unlink(server_queue_name);
}

void client_function(char *server_name, char *client_name) {
    // Create a unique client queue
    char client_queue_name[QUEUE_NAME_SIZE];
    snprintf(client_queue_name, QUEUE_NAME_SIZE, "/chat_%s", client_name);
    mqd_t client_queue = mq_open(client_queue_name, O_RDONLY | O_CREAT | O_NONBLOCK, 0600, NULL);
    if (client_queue == (mqd_t)-1) {
        ERR("mq_open client");
    }

    // Connect to the server
    char server_queue_name[QUEUE_NAME_SIZE];
    snprintf(server_queue_name, QUEUE_NAME_SIZE, "/chat_%s", server_name);
    mqd_t server_queue = mq_open(server_queue_name, O_WRONLY);
    if (server_queue == (mqd_t)-1) {
        ERR("mq_open server");
    }

    // Send a connection message
    send_message(server_queue, client_name, MSG_CONNECT);

    // Register for receiving messages
    register_notification(client_queue);

    // Chat loop
    char message[MSG_SIZE];
    while (1) {
        if (fgets(message, MSG_SIZE, stdin) != NULL) {
            if (strlen(message) == 0) {
                break;
            }

            message[strcspn(message, "\n")] = 0;  // Remove newline character
            char formatted_msg[MSG_SIZE];
            snprintf(formatted_msg, MSG_SIZE, "[%s] %s", client_name, message);
            send_message(server_queue, formatted_msg, MSG_TEXT);
        }
    }

    // Send disconnect message
    send_message(server_queue, "", MSG_DISCONNECT);

    // Cleanup
    mq_close(client_queue);
    mq_unlink(client_queue_name);
    mq_close(server_queue);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_name> <client_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc == 2) {
        // Server
        server_function(argv[1]);
    } else if (argc == 3) {
        // Client
        client_function(argv[1], argv[2]);
    } else {
        fprintf(stderr, "Usage: %s <server_name> <client_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
