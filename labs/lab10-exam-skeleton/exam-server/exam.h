/* exam.h -- GIVEN. Exam protocol and grading with no listener/accept logic. */
#ifndef EXAM_H
#define EXAM_H

#include <stddef.h>
#include "proto.h"

#define MAX_EXAMS 8

typedef struct {
    char name[48];
    char pin[16];
    int exam_id;
} exam_student_t;

typedef struct {
    char text[160];
    char choice[4][64];
    char answer;
} exam_question_t;

typedef struct {
    exam_student_t *students; /* index = id - FIRST_ID */
    int nstudents;
    exam_question_t questions[MAX_EXAMS][MAX_Q];
    int question_count[MAX_EXAMS];
    int exam_count;
} exam_database_t;

typedef struct {
    int logged_in;
    int student_id;
    int exam_id;
} exam_session_t;

int exam_database_load(exam_database_t *database, const char *directory);
void exam_database_free(exam_database_t *database);
void exam_set_database(exam_database_t *database);
void exam_session_init(exam_session_t *session);

/* Handle ONE request line. Write its complete reply to out and return its
 * length. Set *close_after after QUIT. The database is read-only after load;
 * open(O_CREAT|O_EXCL) makes duplicate submission checking atomic. */
size_t exam_handle_request_line(exam_session_t *session, const char *line,
                                char *out, size_t cap, int *close_after);

#endif
