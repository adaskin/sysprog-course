/* exam.h -- GIVEN. The exam service: protocol logic with NO networking.
 *
 * The five servers you write (thread, fork, select, poll, epoll) all call the
 * same function, exam_handle_line(). Only the way they wait for clients differs.
 *
 * Thread-safety: the database is read-only after exam_db_load(). A submission
 * is stored with open(O_CREAT|O_EXCL), so it is atomic between threads AND
 * between processes (no shared memory, no mutex needed). */
#ifndef EXAM_H
#define EXAM_H

#include <stddef.h>

#define MAX_LINE   512      /* longest request line, including the '\n' */
#define OUT_MAX    16384    /* largest reply produced by one request */
#define MAX_EXAMS  8
#define MAX_Q      32
#define FIRST_ID   1001     /* student ids are FIRST_ID .. FIRST_ID+n-1 */

typedef struct {
    char name[48];
    char pin[16];
    int  exam_id;
} student_t;

typedef struct {
    char text[160];
    char choice[4][64];     /* A, B, C, D */
    char answer;            /* 'A'..'D' (never sent to clients before RESULT) */
} question_t;

typedef struct {
    student_t  *students;   /* index = id - FIRST_ID */
    int         nstudents;
    question_t  q[MAX_EXAMS][MAX_Q];
    int         nq[MAX_EXAMS];
    int         nexams;
} exam_db_t;

/* Per-connection state owned by the service. Zero it with session_init(). */
typedef struct {
    int logged_in;
    int student_id;
    int exam_id;
} session_t;

/* Load data/students.csv and data/questions.csv from dir. Returns 0 or -1. */
int  exam_db_load(exam_db_t *db, const char *dir);
void session_init(session_t *s);

/* The service database used by exam_handle_line (set by main.c). */
void        exam_set_db(exam_db_t *db);

/* Handle ONE request line (without the trailing "\n" or "\r\n").
 * Writes the complete reply (every line ends with '\n') into out (capacity
 * cap >= OUT_MAX) and returns its length. Sets *close_after = 1 when the
 * connection must be closed after sending the reply (QUIT).
 * Reads/writes results/<id>.txt. Simulates grading cost with a short sleep
 * (env EXAM_GRADE_DELAY_MS, default 2). Never blocks on the network. */
size_t exam_handle_line(session_t *s, const char *line,
                        char *out, size_t cap, int *close_after);

#endif
