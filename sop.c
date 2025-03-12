#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define MAX_PLAYERS 10
#define MAX_HP 200
#define MAX_ATK 50

typedef struct {
    int id;
    int hp;
    int atk;
} Player;

void read_team(const char *filename, Player team[], int *size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    *size = 0;
    while (fscanf(file, "%d %d %d", &team[*size].id, &team[*size].hp, &team[*size].atk) == 3) {
        (*size)++;
    }
    fclose(file);
}

void battle(int pipeA[], int pipeB[], Player *player) {
    close(pipeA[0]); // Zamykamy odczyt w A
    close(pipeB[1]); // Zamykamy zapis w B

    srand(getpid());
    while (player->hp > 0) {
        int damage = rand() % (player->atk + 1);
        write(pipeA[1], &damage, sizeof(int));
        read(pipeB[0], &damage, sizeof(int));
        player->hp -= damage;
    }

    close(pipeA[1]);
    close(pipeB[0]);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <teamA.txt> <teamB.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    Player teamA[MAX_PLAYERS], teamB[MAX_PLAYERS];
    int sizeA, sizeB;
    read_team(argv[1], teamA, &sizeA);
    read_team(argv[2], teamB, &sizeB);
    
    int pipesA[MAX_PLAYERS][2], pipesB[MAX_PLAYERS][2];
    pid_t pidsA[MAX_PLAYERS], pidsB[MAX_PLAYERS];

    for (int i = 0; i < sizeA; i++) {
        pipe(pipesA[i]);
        pipe(pipesB[i]);
        if ((pidsA[i] = fork()) == 0) {
            battle(pipesA[i], pipesB[i], &teamA[i]);
        }
    }
    for (int i = 0; i < sizeB; i++) {
        if ((pidsB[i] = fork()) == 0) {
            battle(pipesB[i], pipesA[i], &teamB[i]);
        }
    }

    for (int i = 0; i < sizeA; i++) {
        waitpid(pidsA[i], NULL, 0);
    }
    for (int i = 0; i < sizeB; i++) {
        waitpid(pidsB[i], NULL, 0);
    }
    printf("Battle ended!\n");
    return 0;
}
