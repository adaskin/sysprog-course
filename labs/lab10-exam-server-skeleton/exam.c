/* exam.c -- GIVEN. Protocol logic. You do not modify this file. */
#include "exam.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static exam_db_t *g_db;

void exam_set_db(exam_db_t *db) { g_db = db; }
void session_init(session_t *s) { memset(s, 0, sizeof(*s)); s->student_id = -1; }

/* split a CSV line in place into at most max fields; no quoted commas */
static int split_csv(char *line, char **f, int max) {
    int n = 0;
    line[strcspn(line, "\r\n")] = '\0';
    char *p = line;
    while (n < max) {
        f[n++] = p;
        char *c = strchr(p, ',');
        if (!c) break;
        *c = '\0';
        p = c + 1;
    }
    return n;
}

int exam_db_load(exam_db_t *db, const char *dir) {
    char path[256], line[512];
    char *f[10];
    memset(db, 0, sizeof(*db));

    snprintf(path, sizeof(path), "%s/students.csv", dir);
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    int cap = 0;
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }   /* header */
    while (fgets(line, sizeof(line), fp)) {
        if (split_csv(line, f, 10) < 4) continue;
        int id = atoi(f[0]);
        if (id < FIRST_ID) continue;
        int idx = id - FIRST_ID;
        if (idx >= cap) {
            int ncap = (idx + 1) * 2;
            db->students = realloc(db->students, (size_t)ncap * sizeof(student_t));
            if (!db->students) { fclose(fp); return -1; }
            memset(db->students + cap, 0, (size_t)(ncap - cap) * sizeof(student_t));
            cap = ncap;
        }
        snprintf(db->students[idx].name, sizeof(db->students[idx].name), "%s", f[1]);
        snprintf(db->students[idx].pin, sizeof(db->students[idx].pin), "%s", f[2]);
        db->students[idx].exam_id = atoi(f[3]);
        if (idx + 1 > db->nstudents) db->nstudents = idx + 1;
    }
    fclose(fp);

    snprintf(path, sizeof(path), "%s/questions.csv", dir);
    fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }   /* header */
    while (fgets(line, sizeof(line), fp)) {
        if (split_csv(line, f, 10) < 8) continue;
        int e = atoi(f[0]), qn = atoi(f[1]);
        if (e < 0 || e >= MAX_EXAMS || qn < 1 || qn > MAX_Q) continue;
        question_t *q = &db->q[e][qn - 1];
        snprintf(q->text, sizeof(q->text), "%s", f[2]);
        for (int k = 0; k < 4; k++)
            snprintf(q->choice[k], sizeof(q->choice[k]), "%s", f[3 + k]);
        q->answer = f[7][0];
        if (qn > db->nq[e]) db->nq[e] = qn;
        if (e + 1 > db->nexams) db->nexams = e + 1;
    }
    fclose(fp);
    return db->nstudents > 0 && db->nexams > 0 ? 0 : -1;
}

static void grade_delay(void) {
    static int ms = -1;
    if (ms < 0) { const char *e = getenv("EXAM_GRADE_DELAY_MS"); ms = e ? atoi(e) : 2; }
    if (ms > 0) usleep((useconds_t)ms * 1000);
}

static size_t reply(char *out, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static size_t reply(char *out, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return n < 0 ? 0 : ((size_t)n >= cap ? cap - 1 : (size_t)n);
}

static void result_path(char *p, size_t n, int id) {
    snprintf(p, n, "results/%d.txt", id);
}

size_t exam_handle_line(session_t *s, const char *line, char *out, size_t cap, int *close_after) {
    exam_db_t *db = g_db;
    *close_after = 0;
    char cmd[16] = "";
    char arg1[MAX_LINE] = "", arg2[MAX_LINE] = "";
    int n = sscanf(line, "%15s %511s %511s", cmd, arg1, arg2);
    for (char *c = cmd; *c; c++) *c = (char)toupper((unsigned char)*c);
    if (n < 1) return reply(out, cap, "ERR UNKNOWN\n");

    if (!strcmp(cmd, "QUIT")) {
        *close_after = 1;
        return reply(out, cap, "BYE\n");
    }

    if (!strcmp(cmd, "LOGIN")) {
        if (n != 3) return reply(out, cap, "ERR BADFORMAT\n");
        char *end;
        long id = strtol(arg1, &end, 10);
        if (*end || id < FIRST_ID || id - FIRST_ID >= db->nstudents ||
            !db->students[id - FIRST_ID].pin[0] || strcmp(db->students[id - FIRST_ID].pin, arg2))
            return reply(out, cap, "ERR BADLOGIN\n");
        s->logged_in = 1;
        s->student_id = (int)id;
        s->exam_id = db->students[id - FIRST_ID].exam_id;
        return reply(out, cap, "OK %s %d %d\n", db->students[id - FIRST_ID].name,
                     s->exam_id, db->nq[s->exam_id]);
    }

    if (!s->logged_in) return reply(out, cap, "ERR NOLOGIN\n");

    if (!strcmp(cmd, "QUESTIONS")) {
        size_t len = 0;
        for (int i = 0; i < db->nq[s->exam_id]; i++) {
            question_t *q = &db->q[s->exam_id][i];
            len += reply(out + len, cap - len, "Q %d|%s|%s|%s|%s|%s\n", i + 1, q->text,
                         q->choice[0], q->choice[1], q->choice[2], q->choice[3]);
        }
        return len + reply(out + len, cap - len, "END\n");
    }

    if (!strcmp(cmd, "SUBMIT")) {
        int nq = db->nq[s->exam_id];
        if (n != 2 || (int)strlen(arg1) != nq) return reply(out, cap, "ERR BADFORMAT\n");
        for (int i = 0; i < nq; i++) {
            arg1[i] = (char)toupper((unsigned char)arg1[i]);
            if (arg1[i] < 'A' || arg1[i] > 'D') return reply(out, cap, "ERR BADFORMAT\n");
        }
        grade_delay();
        char path[64];
        result_path(path, sizeof(path), s->student_id);
        int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);   /* atomic "first submission wins" */
        if (fd < 0) return reply(out, cap, errno == EEXIST ? "ERR DUPLICATE\n" : "ERR INTERNAL\n");
        char rec[MAX_Q + 2];
        int rl = snprintf(rec, sizeof(rec), "%s\n", arg1);
        ssize_t w = write(fd, rec, (size_t)rl);
        close(fd);
        return reply(out, cap, w == rl ? "OK\n" : "ERR INTERNAL\n");
    }

    if (!strcmp(cmd, "RESULT")) {
        char path[64], ans[MAX_Q + 8] = "";
        result_path(path, sizeof(path), s->student_id);
        FILE *fp = fopen(path, "r");
        if (!fp || !fgets(ans, sizeof(ans), fp)) {
            if (fp) fclose(fp);
            return reply(out, cap, "ERR NOSUBMISSION\n");
        }
        fclose(fp);
        grade_delay();
        int nq = db->nq[s->exam_id], score = 0;
        size_t len = 0;
        char wrong[OUT_MAX / 2] = "";
        size_t wl = 0;
        for (int i = 0; i < nq; i++) {
            char right = db->q[s->exam_id][i].answer;
            if (ans[i] == right) score++;
            else wl += (size_t)snprintf(wrong + wl, sizeof(wrong) - wl, "WRONG %d %c %c\n", i + 1, ans[i], right);
        }
        len += reply(out + len, cap - len, "SCORE %d/%d\n", score, nq);
        len += reply(out + len, cap - len, "%s", wrong);
        return len + reply(out + len, cap - len, "END\n");
    }

    return reply(out, cap, "ERR UNKNOWN\n");
}
