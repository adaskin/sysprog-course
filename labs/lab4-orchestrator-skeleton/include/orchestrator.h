#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <sys/types.h>

#define MAX_TASKS 64
#define MAX_CMD_LEN 512

typedef enum {
    TASK_EMPTY,
    TASK_RUNNING,
    TASK_DONE,
    TASK_ERROR
} TaskState;

typedef struct {
    int id;
    pid_t pid;
    TaskState state;
    int exit_code;
    char command[MAX_CMD_LEN];
} Task;

int task_add(Task *tasks, int *count, pid_t pid, const char *command);
Task *task_find(Task *tasks, int count, int id);

int run_command(Task *tasks, int *count, char **argv);
int wait_for_task(Task *tasks, int count, int id);
void list_tasks(const Task *tasks, int count);
void print_task_status(const Task *tasks, int count, int id);
void print_task_report(int id);
void print_task_log(int id);

#endif
