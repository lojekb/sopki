// klient.c
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSG_SIZE 256
#define QUEUE_NAME_SIZE 64

#define MSG_CONNECT 0
#define MSG_DISCONNECT 1
#define MSG_TEXT 2

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), exit(EXIT_FAILURE))

void send_message(mqd_t queue, const char *message, unsigned int prio);

void send_message(mqd_t queue, const char *message, unsigned int prio) {
    if (mq_send(queue, message, strlen(message) + 1, prio) == -1) {
        ERR("mq_send");
    }
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

    // Chat loop
    char message[MSG_SIZE];
    while (1) {
        if (fgets(message, MSG_SIZE, stdin) != NULL) {
            if (strlen(message) == 0) {
                break;
            }

            message[strcspn(message, "\n")] = 0;  // Remove newline character
            send_message(server_queue, message, MSG_TEXT);
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
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_name> <client_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Run the client function
    client_function(argv[1], argv[2]);

    return EXIT_SUCCESS;
}
