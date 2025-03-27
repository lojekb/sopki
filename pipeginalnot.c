#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define QUEUE_NAME "/example_queue"
#define MSG_SIZE 256
#define NOTIFY_SIGNAL SIGUSR1  // Sygnał do powiadamiania

mqd_t mq;

// Funkcja obsługi sygnału SIGUSR1 (czytanie wiadomości z kolejki)
void handle_signal(int sig) {
    char message[MSG_SIZE];
    unsigned int prio;

    // Odczytaj wiadomość
    if (mq_receive(mq, message, MSG_SIZE, &prio) == -1) {
        perror("mq_receive");
        return;
    }

    printf("Received message: %s\n", message);

    // Ponowna rejestracja powiadomień, ponieważ `mq_notify` jest jednorazowe
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = NOTIFY_SIGNAL;

    if (mq_notify(mq, &sev) == -1) {
        perror("mq_notify");
        exit(EXIT_FAILURE);
    }
}

int main() {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MSG_SIZE;
    attr.mq_curmsgs = 0;

    // Utworzenie kolejki
    mq = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // Ustawienie obsługi sygnału SIGUSR1
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(NOTIFY_SIGNAL, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Rejestracja powiadomień dla kolejki
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = NOTIFY_SIGNAL;

    if (mq_notify(mq, &sev) == -1) {
        perror("mq_notify");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for messages...\n");

    // Program działa w pętli nieskończonej, oczekując na sygnały
    while (1) {
        pause();  // Czeka na sygnały
    }

    // Zamknięcie kolejki (nigdy tu nie dojdziemy w tej wersji kodu)
    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    return 0;
}
