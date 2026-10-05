#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "orchestrator.h"

#define MAX_LINE 1024
#define MAX_ARGS 16

static int parse_line(char *line, char **argv, int max_args) {
    int argc = 0;
    char *saveptr = NULL;

    char *token = strtok_r(line, " \t\n", &saveptr);

    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    argv[argc] = NULL;
    return argc;
}

int main(void) {
    Task tasks[MAX_TASKS];
    int task_count = 0;

    memset(tasks, 0, sizeof(tasks));

    printf("AI Orchestrator Shell\n");
    printf("Type 'help' for commands.\n");

    while (1) {
        printf("aiorch> ");
        fflush(stdout);

        char line[MAX_LINE];

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        char *argv[MAX_ARGS];
        int argc = parse_line(line, argv, MAX_ARGS);

        if (argc == 0) {
            continue;
        }

        if (strcmp(argv[0], "exit") == 0 ||
            strcmp(argv[0], "quit") == 0) {
            break;
        }

        if (strcmp(argv[0], "help") == 0) {
            printf("Commands:\n");
            printf("  run <program> [args...]\n");
            printf("  list\n");
            printf("  status <task_id>\n");
            printf("  wait <task_id>\n");
            printf("  report <task_id>\n");
            printf("  log <task_id>\n");
            printf("  exit\n");
            continue;
        }

        if (strcmp(argv[0], "run") == 0) {
            if (argc < 2) {
                fprintf(stderr, "Usage: run <program> [args...]\n");
                continue;
            }

            run_command(tasks, &task_count, &argv[1]);
            continue;
        }

        if (strcmp(argv[0], "list") == 0) {
            list_tasks(tasks, task_count);
            continue;
        }

        if (strcmp(argv[0], "status") == 0) {
            if (argc < 2) {
                fprintf(stderr, "Usage: status <task_id>\n");
                continue;
            }

            int id = atoi(argv[1]);
            print_task_status(tasks, task_count, id);
            continue;
        }

        if (strcmp(argv[0], "wait") == 0) {
            if (argc < 2) {
                fprintf(stderr, "Usage: wait <task_id>\n");
                continue;
            }

            int id = atoi(argv[1]);
            wait_for_task(tasks, task_count, id);
            continue;
        }

        if (strcmp(argv[0], "report") == 0) {
            if (argc < 2) {
                fprintf(stderr, "Usage: report <task_id>\n");
                continue;
            }

            int id = atoi(argv[1]);
            print_task_report(id);
            continue;
        }

        if (strcmp(argv[0], "log") == 0) {
            if (argc < 2) {
                fprintf(stderr, "Usage: log <task_id>\n");
                continue;
            }

            int id = atoi(argv[1]);
            print_task_log(id);
            continue;
        }

        fprintf(stderr, "Unknown command: %s\n", argv[0]);
    }

    return 0;
}
