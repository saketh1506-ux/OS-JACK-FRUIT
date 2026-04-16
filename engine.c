#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "monitor_ioctl.h"

#define MAX 100

struct container {
    char id[50];
    pid_t pid;
};

struct container containers[MAX];
int count = 0;

// ---------------- START ----------------
void start_container(char *id, char *rootfs, char *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        execl(cmd, cmd, NULL);
        perror("exec failed");
        exit(1);
    } else {
        strcpy(containers[count].id, id);
        containers[count].pid = pid;

        // -------- REGISTER WITH MONITOR --------
        int fd = open("/dev/container_monitor", O_RDWR);
        if (fd >= 0) {
            struct monitor_request req;

            strcpy(req.container_id, id);
            req.pid = pid;
            req.soft_limit_bytes = 5120 * 1024;   // 5MB
            req.hard_limit_bytes = 20480 * 1024;  // 20MB

            ioctl(fd, MONITOR_REGISTER, &req);
            close(fd);
        }

        count++;
        printf("Container started with PID: %d\n", pid);
    }
}

// ---------------- PS ----------------
void list_containers() {
    for (int i = 0; i < count; i++) {
        printf("%s\t%d\n", containers[i].id, containers[i].pid);
    }
}

// ---------------- STOP ----------------
void stop_container(char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGKILL);
            printf("Stopped container %s (PID %d)\n", id, containers[i].pid);
            return;
        }
    }
    printf("Container not found\n");
}

// ---------------- SUPERVISOR ----------------
void run_supervisor() {
    printf("[supervisor] Starting containers...\n");

    pid_t p1 = fork();
    if (p1 == 0) {
        execl("./engine", "./engine", "start", "alpha", "./rootfs", "/bin/sh", NULL);
        exit(0);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        execl("./engine", "./engine", "start", "beta", "./rootfs", "/bin/sh", NULL);
        exit(0);
    }

    wait(NULL);
    wait(NULL);

    printf("[supervisor] Done\n");
}

// ---------------- MAIN ----------------
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: ./engine <command>\n");
        return 1;
    }

    // SUPERVISOR
    if (strcmp(argv[1], "supervisor") == 0) {
        run_supervisor();
        return 0;
    }

    // START
    if (strcmp(argv[1], "start") == 0) {
        if (argc < 5) {
            printf("Usage: ./engine start <id> <rootfs> <cmd>\n");
            return 1;
        }
        start_container(argv[2], argv[3], argv[4]);
    }

    // PS
    else if (strcmp(argv[1], "ps") == 0) {
        list_containers();
    }

    // STOP
    else if (strcmp(argv[1], "stop") == 0) {
        if (argc < 3) {
            printf("Usage: ./engine stop <id>\n");
            return 1;
        }
        stop_container(argv[2]);
    }

    else {
        printf("Unknown command\n");
    }

    return 0;
}
