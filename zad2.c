#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_WORKERS 20
#define TASK_QUEUE_PREFIX "/task_queue_"
#define RESULT_QUEUE_PREFIX "/result_queue_"
#define MSG_SIZE 64
#define MAX_MSG 10

typedef struct {
    double num1;
    double num2;
} Task;

volatile sig_atomic_t stop_signal = 0;

void handle_sigint(int sig) {
    stop_signal = 1;
}

// Funkcja do generowania losowej liczby w podanym zakresie
double random_double(double min, double max) {
    return min + ((double)rand() / RAND_MAX) * (max - min);
}

// Funkcja procesu pracownika
void worker_process(int worker_id, pid_t server_pid) {
    char task_queue_name[32], result_queue_name[32];
    sprintf(task_queue_name, "%s%d", TASK_QUEUE_PREFIX, server_pid);
    sprintf(result_queue_name, "%s%d_%d", RESULT_QUEUE_PREFIX, server_pid, worker_id);

    // Otwórz kolejkę zadań i utwórz własną kolejkę wyników
    mqd_t task_queue = mq_open(task_queue_name, O_RDONLY);
    if (task_queue == (mqd_t)-1) {
        perror("mq_open (task_queue)");
        exit(EXIT_FAILURE);
    }

    struct mq_attr attr = {0, MAX_MSG, MSG_SIZE, 0};
    mqd_t result_queue = mq_open(result_queue_name, O_WRONLY | O_CREAT, 0644, &attr);
    if (result_queue == (mqd_t)-1) {
        perror("mq_open (result_queue)");
        exit(EXIT_FAILURE);
    }

    printf("[%d] Worker ready!\n", getpid());

    for (int i = 0; i < 5; i++) {
        Task task;
        if (mq_receive(task_queue, (char*)&task, sizeof(Task), NULL) == -1) {
            perror("mq_receive");
            continue;
        }
        printf("[%d] Received task [%.2f, %.2f]\n", getpid(), task.num1, task.num2);

        // Symulacja pracy
        usleep((rand() % 1500 + 500) * 1000);

        double result = task.num1 + task.num2;
        printf("[%d] Result [%.2f]\n", getpid(), result);

        if (mq_send(result_queue, (char*)&result, sizeof(double), 0) == -1) {
            perror("mq_send");
        } else {
            printf("[%d] Result sent [%.2f]\n", getpid(), result);
        }
    }

    printf("[%d] Exits\n", getpid());

    mq_close(task_queue);
    mq_close(result_queue);
    mq_unlink(result_queue_name);
    exit(0);
}

// Funkcja procesu serwera
void server_process(int num_workers, int t1, int t2) {
    char task_queue_name[32];
    sprintf(task_queue_name, "%s%d", TASK_QUEUE_PREFIX, getpid());

    struct mq_attr attr = {0, MAX_MSG, MSG_SIZE, 0};
    mqd_t task_queue = mq_open(task_queue_name, O_WRONLY | O_CREAT, 0644, &attr);
    if (task_queue == (mqd_t)-1) {
        perror("mq_open (task_queue)");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);

    printf("Server is starting...\n");

    pid_t workers[num_workers];
    for (int i = 0; i < num_workers; i++) {
        if ((workers[i] = fork()) == 0) {
            worker_process(i, getpid());
        }
    }

    srand(time(NULL));

    for (int i = 0; i < num_workers * 5; i++) {
        if (stop_signal) break;

        usleep((rand() % (t2 - t1) + t1) * 1000);

        Task task = {random_double(0.0, 100.0), random_double(0.0, 100.0)};
        if (mq_send(task_queue, (char*)&task, sizeof(Task), 0) == -1) {
            printf("Queue is full!\n");
        } else {
            printf("New task queued: [%.2f, %.2f]\n", task.num1, task.num2);
        }
    }

    // Oczekiwanie na zakończenie pracowników
    for (int i = 0; i < num_workers; i++) {
        waitpid(workers[i], NULL, 0);
    }

    printf("All child processes have finished.\n");

    mq_close(task_queue);
    mq_unlink(task_queue_name);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_workers> <T1> <T2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_workers = atoi(argv[1]);
    int t1 = atoi(argv[2]);
    int t2 = atoi(argv[3]);

    if (num_workers < 2 || num_workers > 20 || t1 < 100 || t2 > 5000 || t1 >= t2) {
        fprintf(stderr, "Invalid arguments. Constraints: 2 <= num_workers <= 20, 100 <= T1 < T2 <= 5000\n");
        exit(EXIT_FAILURE);
    }

    server_process(num_workers, t1, t2);

    return 0;
}
