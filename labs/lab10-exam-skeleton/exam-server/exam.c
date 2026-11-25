/* exam.c -- GIVEN. Protocol logic and safe input handling. Do not modify. */
#include "exam.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static exam_db_t *g_db;

void exam_set_db(exam_db_t *db) { g_db = db; }
void session_init(session_t *s) { memset(s, 0, sizeof(*s)); s->student_id = -1; }

/* GIVEN: checked integer conversion; rejects junk, overflow, and bad ranges. */
static int parse_int(const char *text, int min, int max, int *out)
{
    char *end;
    if (!text || !*text) return -1;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value < min || value > max)
        return -1;
    *out = (int)value;
    return 0;
}

static int copy_field(char *dst, size_t cap, const char *src)
{
    size_t len = strlen(src);
    if (len >= cap) {
        errno = EMSGSIZE;
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

static int data_path(char *path, size_t cap, const char *dir,
                     const char *filename)
{
    int n = snprintf(path, cap, "%s/%s", dir, filename);
    if (n < 0 || (size_t)n >= cap) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

/* Read one complete CSV row. Overlong rows are errors, not truncated records. */
static int read_csv_row(FILE *fp, char *line, size_t cap)
{
    if (!fgets(line, (int)cap, fp)) return ferror(fp) ? -1 : 0;
    size_t len = strlen(line);
    if ((len > 0 && line[len - 1] == '\n') || feof(fp)) return 1;

    int ch;
    do { ch = fgetc(fp); } while (ch != '\n' && ch != EOF);
    errno = EMSGSIZE;
    return -1;
}

static int split_csv(char *line, char **f, int max)
{
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

static int load_error(exam_db_t *db, FILE *fp)
{
    if (fp) fclose(fp);
    free(db->students);
    db->students = NULL;
    db->nstudents = 0;
    return -1;
}

int exam_db_load(exam_db_t *db, const char *dir)
{
    char path[256], line[512];
    char *f[10];
    memset(db, 0, sizeof(*db));

    if (data_path(path, sizeof(path), dir, "students.csv") < 0) {
        perror("students.csv path");
        return -1;
    }
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    if (read_csv_row(fp, line, sizeof(line)) <= 0) return load_error(db, fp);

    int cap = 0, rc;
    while ((rc = read_csv_row(fp, line, sizeof(line))) > 0) {
        if (split_csv(line, f, 10) < 4) continue;
        int id, exam_id;
        if (parse_int(f[0], FIRST_ID, FIRST_ID + MAX_STUDENTS - 1, &id) < 0 ||
            parse_int(f[3], 0, MAX_EXAMS - 1, &exam_id) < 0)
            continue;

        int idx = id - FIRST_ID;
        if (idx >= cap) {
            int ncap = (idx + 1) * 2;
            student_t *grown = realloc(db->students,
                                       (size_t)ncap * sizeof(*grown));
            if (!grown) return load_error(db, fp);
            db->students = grown;
            memset(db->students + cap, 0,
                   (size_t)(ncap - cap) * sizeof(*db->students));
            cap = ncap;
        }
        if (copy_field(db->students[idx].name,
                       sizeof(db->students[idx].name), f[1]) < 0 ||
            copy_field(db->students[idx].pin,
                       sizeof(db->students[idx].pin), f[2]) < 0)
            return load_error(db, fp);
        db->students[idx].exam_id = exam_id;
        if (idx + 1 > db->nstudents) db->nstudents = idx + 1;
    }
    if (rc < 0) return load_error(db, fp);
    fclose(fp);

    if (data_path(path, sizeof(path), dir, "questions.csv") < 0) {
        perror("questions.csv path");
        return load_error(db, NULL);
    }
    fp = fopen(path, "r");
    if (!fp) { perror(path); return load_error(db, NULL); }
    if (read_csv_row(fp, line, sizeof(line)) <= 0) return load_error(db, fp);

    while ((rc = read_csv_row(fp, line, sizeof(line))) > 0) {
        if (split_csv(line, f, 10) < 8) continue;
        int exam_id, qn;
        if (parse_int(f[0], 0, MAX_EXAMS - 1, &exam_id) < 0 ||
            parse_int(f[1], 1, MAX_Q, &qn) < 0)
            continue;
        if (strlen(f[7]) != 1 || f[7][0] < 'A' || f[7][0] > 'D') continue;

        question_t *q = &db->q[exam_id][qn - 1];
        if (copy_field(q->text, sizeof(q->text), f[2]) < 0)
            return load_error(db, fp);
        for (int k = 0; k < 4; k++) {
            if (copy_field(q->choice[k], sizeof(q->choice[k]), f[3 + k]) < 0)
                return load_error(db, fp);
        }
        q->answer = f[7][0];
        if (qn > db->nq[exam_id]) db->nq[exam_id] = qn;
        if (exam_id + 1 > db->nexams) db->nexams = exam_id + 1;
    }
    if (rc < 0) return load_error(db, fp);
    fclose(fp);
    return db->nstudents > 0 && db->nexams > 0 ? 0 : load_error(db, NULL);
}

static void grade_delay(void)
{
    /* Read the setting each time: no lazy static initialization race between
     * threads. getenv() is safe here because the program never calls setenv(). */
    const char *text = getenv("EXAM_GRADE_DELAY_MS");
    int ms = 2;
    if (text && parse_int(text, 0, 60000, &ms) < 0) ms = 2;
    if (ms > 0) usleep((useconds_t)ms * 1000);
}

/* Return the byte count, or cap if formatting failed/would truncate. */
static size_t reply(char *out, size_t cap, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
static size_t reply(char *out, size_t cap, const char *fmt, ...)
{
    if (cap == 0) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return cap;
    }
    return (size_t)n;
}

static int result_path(char *path, size_t cap, int id)
{
    int n = snprintf(path, cap, "results/%d.txt", id);
    if (n < 0 || (size_t)n >= cap) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

size_t exam_handle_line(session_t *s, const char *line, char *out,
                        size_t cap, int *close_after)
{
    exam_db_t *db = g_db;
    *close_after = 0;
    char cmd[16] = "";
    char arg1[MAX_LINE] = "", arg2[MAX_LINE] = "", extra;
    int n = sscanf(line, "%15s %511s %511s %c", cmd, arg1, arg2, &extra);
    for (char *c = cmd; *c; c++) *c = (char)toupper((unsigned char)*c);
    if (n < 1) return reply(out, cap, "ERR UNKNOWN\n");

    if (!strcmp(cmd, "QUIT")) {
        if (n != 1) return reply(out, cap, "ERR BADFORMAT\n");
        *close_after = 1;
        return reply(out, cap, "BYE\n");
    }

    if (!strcmp(cmd, "LOGIN")) {
        int id;
        if (n != 3) return reply(out, cap, "ERR BADFORMAT\n");
        if (parse_int(arg1, FIRST_ID, FIRST_ID + MAX_STUDENTS - 1, &id) < 0 ||
            !db->students[id - FIRST_ID].pin[0] ||
            strcmp(db->students[id - FIRST_ID].pin, arg2))
            return reply(out, cap, "ERR BADLOGIN\n");
        s->logged_in = 1;
        s->student_id = id;
        s->exam_id = db->students[id - FIRST_ID].exam_id;
        return reply(out, cap, "OK %s %d %d\n",
                     db->students[id - FIRST_ID].name,
                     s->exam_id, db->nq[s->exam_id]);
    }

    if (!s->logged_in) return reply(out, cap, "ERR NOLOGIN\n");

    if (!strcmp(cmd, "QUESTIONS")) {
        if (n != 1) return reply(out, cap, "ERR BADFORMAT\n");
        size_t len = 0;
        for (int i = 0; i < db->nq[s->exam_id]; i++) {
            question_t *q = &db->q[s->exam_id][i];
            size_t left = cap - len;
            size_t wrote = reply(out + len, left, "Q %d|%s|%s|%s|%s|%s\n",
                                 i + 1, q->text, q->choice[0], q->choice[1],
                                 q->choice[2], q->choice[3]);
            if (wrote >= left) return cap;
            len += wrote;
        }
        size_t left = cap - len;
        size_t wrote = reply(out + len, left, "END\n");
        return wrote >= left ? cap : len + wrote;
    }

    if (!strcmp(cmd, "SUBMIT")) {
        int nq = db->nq[s->exam_id];
        if (n != 2 || (int)strlen(arg1) != nq)
            return reply(out, cap, "ERR BADFORMAT\n");
        for (int i = 0; i < nq; i++) {
            arg1[i] = (char)toupper((unsigned char)arg1[i]);
            if (arg1[i] < 'A' || arg1[i] > 'D')
                return reply(out, cap, "ERR BADFORMAT\n");
        }
        grade_delay();
        char path[64];
        if (result_path(path, sizeof(path), s->student_id) < 0)
            return reply(out, cap, "ERR INTERNAL\n");
        int fd = open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0644);
        if (fd < 0)
            return reply(out, cap, errno == EEXIST ?
                         "ERR DUPLICATE\n" : "ERR INTERNAL\n");
        char rec[MAX_Q + 2];
        int rl = snprintf(rec, sizeof(rec), "%s\n", arg1);
        if (rl < 0 || (size_t)rl >= sizeof(rec)) {
            close(fd);
            unlink(path);
            return reply(out, cap, "ERR INTERNAL\n");
        }

        size_t done = 0;
        int ok = 1;
        while (done < (size_t)rl) {
            ssize_t w = write(fd, rec + done, (size_t)rl - done);
            if (w > 0) done += (size_t)w;
            else if (w < 0 && errno == EINTR) continue;
            else { ok = 0; break; }
        }
        if (close(fd) < 0) ok = 0;
        if (!ok) unlink(path);
        return reply(out, cap, ok ? "OK\n" : "ERR INTERNAL\n");
    }

    if (!strcmp(cmd, "RESULT")) {
        if (n != 1) return reply(out, cap, "ERR BADFORMAT\n");
        char path[64], ans[MAX_Q + 2];
        if (result_path(path, sizeof(path), s->student_id) < 0)
            return reply(out, cap, "ERR INTERNAL\n");
        FILE *fp = fopen(path, "r");
        if (!fp || !fgets(ans, sizeof(ans), fp)) {
            if (fp) fclose(fp);
            return reply(out, cap, "ERR NOSUBMISSION\n");
        }
        fclose(fp);
        ans[strcspn(ans, "\r\n")] = '\0';
        int nq = db->nq[s->exam_id];
        if ((int)strlen(ans) != nq) return reply(out, cap, "ERR INTERNAL\n");

        grade_delay();
        int score = 0;
        for (int i = 0; i < nq; i++) {
            if (ans[i] == db->q[s->exam_id][i].answer) score++;
        }

        size_t len = 0;
        size_t left = cap;
        size_t wrote = reply(out, left, "SCORE %d/%d\n", score, nq);
        if (wrote >= left) return cap;
        len = wrote;

        for (int i = 0; i < nq; i++) {
            char right = db->q[s->exam_id][i].answer;
            if (ans[i] == right) continue;
            left = cap - len;
            wrote = reply(out + len, left, "WRONG %d %c %c\n",
                          i + 1, ans[i], right);
            if (wrote >= left) return cap;
            len += wrote;
        }

        left = cap - len;
        wrote = reply(out + len, left, "END\n");
        return wrote >= left ? cap : len + wrote;
    }

    return reply(out, cap, "ERR UNKNOWN\n");
}
