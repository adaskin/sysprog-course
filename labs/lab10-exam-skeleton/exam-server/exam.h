/* exam.h -- GIVEN. Exam protocol and grading with NO networking. */
#ifndef EXAM_H
#define EXAM_H

#include <stddef.h>
#include "proto.h"

#define MAX_EXAMS 8

typedef struct {
    char name[48];
    char pin[16];
    int exam_id;
} student_t;

typedef struct {
    char text[160];
    char choice[4][64];
    char answer;
} question_t;

typedef struct {
    student_t *students;    /* index = id - FIRST_ID */
    int nstudents;
    question_t q[MAX_EXAMS][MAX_Q];
    int nq[MAX_EXAMS];
    int nexams;
} exam_db_t;

typedef struct {
    int logged_in;
    int student_id;
    int exam_id;
} session_t;

int exam_db_load(exam_db_t *db, const char *dir);
void session_init(session_t *s);
void exam_set_db(exam_db_t *db);

/* Handle ONE request line. Write its complete reply to out and return its
 * length. Set *close_after after QUIT. The database is read-only after load;
 * open(O_CREAT|O_EXCL) makes duplicate submission checking atomic. */
size_t exam_handle_line(session_t *s, const char *line,
                        char *out, size_t cap, int *close_after);

#endif
