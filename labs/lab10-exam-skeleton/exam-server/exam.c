/* exam.c -- GIVEN. Request dispatch, protocol logic, grading, and
 * safe input. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"

static exam_database_t* g_database;

void exam_set_database(exam_database_t* database) {
    g_database = database;
}

void exam_session_init(exam_session_t* session) {
    memset(session, 0, sizeof(*session));
    session->student_id = -1;
}

void exam_database_free(exam_database_t* database) {
    if (!database) return;
    free(database->students);
    memset(database, 0, sizeof(*database));
    if (g_database == database) g_database = NULL;
}

static int parse_int_in_range(const char* text, int min, int max,
                              int* out) {
    char* end;
    if (!text || !*text) return -1;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value < min || value > max)
        return -1;
    *out = (int)value;
    return 0;
}

static int copy_bounded_string(char* destination, size_t cap,
                               const char* source) {
    size_t len = strlen(source);
    if (len >= cap) {
        errno = EMSGSIZE;
        return -1;
    }
    memcpy(destination, source, len + 1);
    return 0;
}

static int build_data_path(char* path, size_t cap,
                           const char* directory,
                           const char* filename) {
    int n = snprintf(path, cap, "%s/%s", directory, filename);
    if (n < 0 || (size_t)n >= cap) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

/* Read one complete CSV row. Overlong rows are errors, not truncated
 * records. */
static int read_csv_row(FILE* file, char* line, size_t cap) {
    if (!fgets(line, (int)cap, file)) return ferror(file) ? -1 : 0;
    size_t len = strlen(line);
    if ((len > 0 && line[len - 1] == '\n') || feof(file)) return 1;

    int ch;
    do {
        ch = fgetc(file);
    } while (ch != '\n' && ch != EOF);
    errno = EMSGSIZE;
    return -1;
}

static int split_csv(char* line, char** fields, int max) {
    int count = 0;
    line[strcspn(line, "\r\n")] = '\0';
    char* position = line;
    while (count < max) {
        fields[count++] = position;
        char* comma = strchr(position, ',');
        if (!comma) break;
        *comma = '\0';
        position = comma + 1;
    }
    return count;
}

static int exam_load_failed(exam_database_t* database, FILE* file) {
    if (file) fclose(file);
    exam_database_free(database);
    return -1;
}

int exam_database_load(exam_database_t* database,
                       const char* directory) {
    char path[256], line[512];
    char* fields[10];
    memset(database, 0, sizeof(*database));

    if (build_data_path(path, sizeof(path), directory,
                        "students.csv") < 0) {
        perror("students.csv path");
        return -1;
    }
    FILE* file = fopen(path, "r");
    if (!file) {
        perror(path);
        return -1;
    }
    if (read_csv_row(file, line, sizeof(line)) <= 0)
        return exam_load_failed(database, file);

    int capacity = 0, rc;
    while ((rc = read_csv_row(file, line, sizeof(line))) > 0) {
        if (split_csv(line, fields, 10) < 4) continue;
        int id, exam_id;
        if (parse_int_in_range(fields[0], FIRST_ID,
                               FIRST_ID + MAX_STUDENTS - 1,
                               &id) < 0 ||
            parse_int_in_range(fields[3], 0, MAX_EXAMS - 1,
                               &exam_id) < 0)
            continue;

        int index = id - FIRST_ID;
        if (index >= capacity) {
            int new_capacity = (index + 1) * 2;
            exam_student_t* grown =
                realloc(database->students,
                        (size_t)new_capacity * sizeof(*grown));
            if (!grown) return exam_load_failed(database, file);
            database->students = grown;
            memset(
                database->students + capacity, 0,
                (size_t)(new_capacity - capacity) * sizeof(*grown));
            capacity = new_capacity;
        }
        if (copy_bounded_string(
                database->students[index].name,
                sizeof(database->students[index].name),
                fields[1]) < 0 ||
            copy_bounded_string(database->students[index].pin,
                                sizeof(database->students[index].pin),
                                fields[2]) < 0)
            return exam_load_failed(database, file);
        database->students[index].exam_id = exam_id;
        if (index + 1 > database->nstudents)
            database->nstudents = index + 1;
    }
    if (rc < 0) return exam_load_failed(database, file);
    fclose(file);

    if (build_data_path(path, sizeof(path), directory,
                        "questions.csv") < 0) {
        perror("questions.csv path");
        return exam_load_failed(database, NULL);
    }
    file = fopen(path, "r");
    if (!file) {
        perror(path);
        return exam_load_failed(database, NULL);
    }
    if (read_csv_row(file, line, sizeof(line)) <= 0)
        return exam_load_failed(database, file);

    while ((rc = read_csv_row(file, line, sizeof(line))) > 0) {
        if (split_csv(line, fields, 10) < 8) continue;
        int exam_id, question_number;
        if (parse_int_in_range(fields[0], 0, MAX_EXAMS - 1,
                               &exam_id) < 0 ||
            parse_int_in_range(fields[1], 1, MAX_Q,
                               &question_number) < 0)
            continue;
        if (strlen(fields[7]) != 1 || fields[7][0] < 'A' ||
            fields[7][0] > 'D')
            continue;

        exam_question_t* question =
            &database->questions[exam_id][question_number - 1];
        if (copy_bounded_string(question->text,
                                sizeof(question->text),
                                fields[2]) < 0)
            return exam_load_failed(database, file);
        for (int k = 0; k < 4; k++) {
            if (copy_bounded_string(question->choice[k],
                                    sizeof(question->choice[k]),
                                    fields[3 + k]) < 0)
                return exam_load_failed(database, file);
        }
        question->answer = fields[7][0];
        if (question_number > database->question_count[exam_id])
            database->question_count[exam_id] = question_number;
        if (exam_id + 1 > database->exam_count)
            database->exam_count = exam_id + 1;
    }
    if (rc < 0) return exam_load_failed(database, file);
    fclose(file);
    return database->nstudents > 0 && database->exam_count > 0
               ? 0
               : exam_load_failed(database, NULL);
}

static void apply_grading_delay(void) {
    /* No lazy static initialization race between worker threads. */
    const char* text = getenv("EXAM_GRADE_DELAY_MS");
    int milliseconds = 2;
    if (text && parse_int_in_range(text, 0, 60000, &milliseconds) < 0)
        milliseconds = 2;
    if (milliseconds > 0) usleep((useconds_t)milliseconds * 1000);
}

/* Return the byte count, or cap if formatting failed/would truncate.
 */
static size_t format_protocol_reply(char* out, size_t cap,
                                    const char* format, ...)
    __attribute__((format(printf, 3, 4)));
static size_t format_protocol_reply(char* out, size_t cap,
                                    const char* format, ...) {
    if (cap == 0) return 0;
    va_list arguments;
    va_start(arguments, format);
    int n = vsnprintf(out, cap, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return cap;
    }
    return (size_t)n;
}

static int build_result_path(char* path, size_t cap, int id) {
    int n = snprintf(path, cap, "results/%d.txt", id);
    if (n < 0 || (size_t)n >= cap) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

size_t exam_handle_request_line(exam_session_t* session,
                                const char* line, char* out,
                                size_t cap, int* close_after) {
    exam_database_t* database = g_database;
    *close_after = 0;
    char command[16] = "";
    char arg1[MAX_LINE] = "", arg2[MAX_LINE] = "", extra;
    int n = sscanf(line, "%15s %511s %511s %c", command, arg1, arg2,
                   &extra);
    for (char* c = command; *c; c++)
        *c = (char)toupper((unsigned char)*c);
    if (n < 1)
        return format_protocol_reply(out, cap, "ERR UNKNOWN\n");

    if (!strcmp(command, "QUIT")) {
        if (n != 1)
            return format_protocol_reply(out, cap, "ERR BADFORMAT\n");
        *close_after = 1;
        return format_protocol_reply(out, cap, "BYE\n");
    }

    if (!strcmp(command, "LOGIN")) {
        int id;
        if (n != 3)
            return format_protocol_reply(out, cap, "ERR BADFORMAT\n");
        if (parse_int_in_range(arg1, FIRST_ID,
                               FIRST_ID + MAX_STUDENTS - 1,
                               &id) < 0 ||
            !database->students[id - FIRST_ID].pin[0] ||
            strcmp(database->students[id - FIRST_ID].pin, arg2))
            return format_protocol_reply(out, cap, "ERR BADLOGIN\n");
        session->logged_in = 1;
        session->student_id = id;
        session->exam_id = database->students[id - FIRST_ID].exam_id;
        return format_protocol_reply(
            out, cap, "OK %s %d %d\n",
            database->students[id - FIRST_ID].name, session->exam_id,
            database->question_count[session->exam_id]);
    }

    if (!session->logged_in)
        return format_protocol_reply(out, cap, "ERR NOLOGIN\n");

    if (!strcmp(command, "QUESTIONS")) {
        if (n != 1)
            return format_protocol_reply(out, cap, "ERR BADFORMAT\n");
        size_t len = 0;
        for (int i = 0;
             i < database->question_count[session->exam_id]; i++) {
            exam_question_t* question =
                &database->questions[session->exam_id][i];
            size_t left = cap - len;
            size_t wrote = format_protocol_reply(
                out + len, left, "Q %d|%s|%s|%s|%s|%s\n", i + 1,
                question->text, question->choice[0],
                question->choice[1], question->choice[2],
                question->choice[3]);
            if (wrote >= left) return cap;
            len += wrote;
        }
        size_t left = cap - len;
        size_t wrote =
            format_protocol_reply(out + len, left, "END\n");
        return wrote >= left ? cap : len + wrote;
    }

    if (!strcmp(command, "SUBMIT")) {
        int question_count =
            database->question_count[session->exam_id];
        if (n != 2 || (int)strlen(arg1) != question_count)
            return format_protocol_reply(out, cap, "ERR BADFORMAT\n");
        for (int i = 0; i < question_count; i++) {
            arg1[i] = (char)toupper((unsigned char)arg1[i]);
            if (arg1[i] < 'A' || arg1[i] > 'D')
                return format_protocol_reply(out, cap,
                                             "ERR BADFORMAT\n");
        }
        apply_grading_delay();
        char path[64];
        if (build_result_path(path, sizeof(path),
                              session->student_id) < 0)
            return format_protocol_reply(out, cap, "ERR INTERNAL\n");
        int fd =
            open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0644);
        if (fd < 0)
            return format_protocol_reply(out, cap,
                                         errno == EEXIST
                                             ? "ERR DUPLICATE\n"
                                             : "ERR INTERNAL\n");
        char record[MAX_Q + 2];
        int record_len =
            snprintf(record, sizeof(record), "%s\n", arg1);
        if (record_len < 0 || (size_t)record_len >= sizeof(record)) {
            close(fd);
            unlink(path);
            return format_protocol_reply(out, cap, "ERR INTERNAL\n");
        }

        size_t done = 0;
        int ok = 1;
        while (done < (size_t)record_len) {
            ssize_t written =
                write(fd, record + done, (size_t)record_len - done);
            if (written > 0)
                done += (size_t)written;
            else if (written < 0 && errno == EINTR)
                continue;
            else {
                ok = 0;
                break;
            }
        }
        if (close(fd) < 0) ok = 0;
        if (!ok) unlink(path);
        return format_protocol_reply(out, cap,
                                     ok ? "OK\n" : "ERR INTERNAL\n");
    }

    if (!strcmp(command, "RESULT")) {
        if (n != 1)
            return format_protocol_reply(out, cap, "ERR BADFORMAT\n");
        char path[64], answers[MAX_Q + 2];
        if (build_result_path(path, sizeof(path),
                              session->student_id) < 0)
            return format_protocol_reply(out, cap, "ERR INTERNAL\n");
        FILE* file = fopen(path, "r");
        if (!file || !fgets(answers, sizeof(answers), file)) {
            if (file) fclose(file);
            return format_protocol_reply(out, cap,
                                         "ERR NOSUBMISSION\n");
        }
        fclose(file);
        answers[strcspn(answers, "\r\n")] = '\0';
        int question_count =
            database->question_count[session->exam_id];
        if ((int)strlen(answers) != question_count)
            return format_protocol_reply(out, cap, "ERR INTERNAL\n");

        apply_grading_delay();
        int score = 0;
        for (int i = 0; i < question_count; i++) {
            if (answers[i] ==
                database->questions[session->exam_id][i].answer)
                score++;
        }

        size_t len = 0;
        size_t left = cap;
        size_t wrote = format_protocol_reply(
            out, left, "SCORE %d/%d\n", score, question_count);
        if (wrote >= left) return cap;
        len = wrote;

        for (int i = 0; i < question_count; i++) {
            char right =
                database->questions[session->exam_id][i].answer;
            if (answers[i] == right) continue;
            left = cap - len;
            wrote = format_protocol_reply(out + len, left,
                                          "WRONG %d %c %c\n", i + 1,
                                          answers[i], right);
            if (wrote >= left) return cap;
            len += wrote;
        }

        left = cap - len;
        wrote = format_protocol_reply(out + len, left, "END\n");
        return wrote >= left ? cap : len + wrote;
    }

    return format_protocol_reply(out, cap, "ERR UNKNOWN\n");
}

/* GIVEN: process every complete request currently buffered. The line
 * buffer retains partial input and any later requests for the next
 * call. */
int exam_process_buffered_requests(connection_t* connection) {
    char line[MAX_LINE + 1];
    char out[OUT_MAX];

    for (;;) {
        int r = line_buffer_next_line(&connection->input, line,
                                      sizeof(line));
        if (r == 0) return 0; /* need more bytes */
        if (r < 0) {
            if (errno == EMSGSIZE) {
                static const char message[] = "ERR TOOLONG\n";
                (void)socket_send_all(connection->input.fd, message,
                                      sizeof(message) - 1);
            }
            return -1;
        }

        int close_after = 0;
        size_t len =
            exam_handle_request_line(&connection->session, line, out,
                                     sizeof(out), &close_after);
        if (len >= sizeof(out)) {
            static const char message[] = "ERR INTERNAL\n";
            memcpy(out, message, sizeof(message) - 1);
            len = sizeof(message) - 1;
        }
        if (socket_send_all(connection->input.fd, out, len) < 0)
            return -1;
        if (close_after) return -1;
    }
}
