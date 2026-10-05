#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "orchestrator.h"

int task_add(Task *tasks, int *count, pid_t pid, const char *command) {
    if (*count >= MAX_TASKS) {
        fprintf(stderr, "Too many tasks\n");
        return -1;
    }

    int id = *count + 1;

    tasks[*count].id = id;
    tasks[*count].pid = pid;
    tasks[*count].state = TASK_RUNNING;
    tasks[*count].exit_code = -1;

    snprintf(tasks[*count].command,
             sizeof(tasks[*count].command),
             "%s",
             command);

    (*count)++;

    return id;
}

Task *task_find(Task *tasks, int count, int id) {
    for (int i = 0; i < count; i++) {
        if (tasks[i].id == id) {
            return &tasks[i];
        }
    }

    return NULL;
}

static void write_status_file(int id, pid_t pid, const char *state) {
    char path[256];

    snprintf(path, sizeof(path), "status/task_%03d.status", id);

    FILE *f = fopen(path, "w");

    if (!f) {
        perror("fopen status file failed");
        return;
    }

    fprintf(f, "task_id: %d\n", id);
    fprintf(f, "pid: %d\n", pid);
    fprintf(f, "state: %s\n", state);

    fclose(f);
}

int run_command(Task *tasks, int *count, char **argv) {
    if (*count >= MAX_TASKS) {
        fprintf(stderr, "Task table full (MAX_TASKS=%d)\n", MAX_TASKS);
        return -1;
    }

    int next_id = *count + 1;

    char logpath[256];
    snprintf(logpath, sizeof(logpath), "logs/task_%03d.log", next_id);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        /* Child process: the agent */

        /* TODO (Task 2):
           1. open(logpath, O_WRONLY | O_CREAT | O_TRUNC, 0644)
           2. dup2 the log fd onto STDOUT_FILENO and STDERR_FILENO
           3. close the original log fd
           4. then execvp below runs the agent with its voice in the log
        */

        execvp(argv[0], argv);

        perror("execvp failed");
        _exit(127);
    }

    /* Parent process: the orchestrator */

    char command[MAX_CMD_LEN] = "";

    for (int i = 0; argv[i] != NULL; i++) {
        strncat(command, argv[i],
                sizeof(command) - strlen(command) - 2);
        strcat(command, " ");
    }

    int id = task_add(tasks, count, pid, command);

    if (id < 0) {
        return -1;
    }

    write_status_file(id, pid, "running");

    printf("Started task %d\n", id);
    printf("PID: %d\n", pid);

    return id;
}

int wait_for_task(Task *tasks, int count, int id) {
    Task *task = task_find(tasks, count, id);

    if (!task) {
        fprintf(stderr, "Task %d not found\n", id);
        return -1;
    }

    if (task->state != TASK_RUNNING) {
        printf("Task %d already finished.\n", id);
        return 0;
    }

    int status;

    if (waitpid(task->pid, &status, 0) < 0) {
        perror("waitpid failed");
        task->state = TASK_ERROR;
        return -1;
    }

    if (WIFEXITED(status)) {
        task->exit_code = WEXITSTATUS(status);
        task->state = TASK_DONE;

        printf("Task %d finished with exit code %d\n",
               task->id,
               task->exit_code);
    } else if (WIFSIGNALED(status)) {
        task->state = TASK_ERROR;
        printf("Task %d killed by signal %d\n", task->id, WTERMSIG(status));
    } else {
        task->state = TASK_ERROR;
        printf("Task %d terminated abnormally\n", task->id);
    }

    /* TODO (Task 5, optional): rewrite the status file with the
       final state and exit code (state: done / error, exit_code: N) */

    return 0;
}

void list_tasks(const Task *tasks, int count) {
    printf("%-4s %-10s %-10s %s\n",
           "ID", "PID", "STATE", "COMMAND");

    for (int i = 0; i < count; i++) {
        const char *state = "UNKNOWN";

        if (tasks[i].state == TASK_RUNNING) {
            state = "RUNNING";
        } else if (tasks[i].state == TASK_DONE) {
            state = "DONE";
        } else if (tasks[i].state == TASK_ERROR) {
            state = "ERROR";
        }

        printf("%-4d %-10d %-10s %s\n",
               tasks[i].id,
               tasks[i].pid,
               state,
               tasks[i].command);
    }
}

void print_task_status(const Task *tasks, int count, int id) {
    const Task *task = NULL;

    for (int i = 0; i < count; i++) {
        if (tasks[i].id == id) {
            task = &tasks[i];
            break;
        }
    }

    if (!task) {
        fprintf(stderr, "Task %d not found\n", id);
        return;
    }

    printf("Task ID: %d\n", task->id);
    printf("PID: %d\n", task->pid);
    printf("Command: %s\n", task->command);

    if (task->state == TASK_RUNNING) {
        printf("State: RUNNING\n");
    } else if (task->state == TASK_DONE) {
        printf("State: DONE\n");
        printf("Exit code: %d\n", task->exit_code);
    } else {
        printf("State: ERROR\n");
    }
}

void print_task_report(int id) {
    char path[256];

    snprintf(path, sizeof(path), "reports/task_%03d.report", id);

    FILE *f = fopen(path, "r");

    if (!f) {
        fprintf(stderr, "No report available for task %d\n", id);
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);
}

void print_task_log(int id) {
    char path[256];

    snprintf(path, sizeof(path), "logs/task_%03d.log", id);

    FILE *f = fopen(path, "r");

    if (!f) {
        fprintf(stderr, "No log available for task %d\n", id);
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);
}
