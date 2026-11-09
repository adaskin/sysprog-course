/* protocol.c — parse and format protocol lines (given). */
#include "protocol.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOK 8

static int split(char *s, char *tok[], int max) {
    int n = 0;
    char *save = NULL;                                /* strtok_r: several threads parse at once */
    for (char *p = strtok_r(s, " ", &save); p; p = strtok_r(NULL, " ", &save)) {
        if (n == max) {
            return -1;
        }
        tok[n++] = p;
    }
    return n;
}

static int to_int(const char *s, int lo, int hi, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v < lo || v > hi) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int to_word(const char *s, char *out, size_t n) {
    size_t len = strlen(s);
    if (len == 0 || len >= n) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        if (s[i] < '!' || s[i] > '~') {
            return -1;
        }
    }
    memcpy(out, s, len + 1);
    return 0;
}

int proto_parse(const char *line, Msg *m) {
    char copy[MAX_LINE];
    if (strlen(line) >= sizeof(copy)) {
        return -1;
    }
    memset(m, 0, sizeof(*m));
    strcpy(copy, line);
    char *t[MAX_TOK + 1];
    int n = split(copy, t, MAX_TOK + 1);
    if (n < 1 || n > MAX_TOK) {
        return -1;
    }
    const char *k = t[0];
    int ok = 0;
    int BIG = 1 << 20;

    if (strcmp(k, "ASSIGN") == 0) {
        m->type = MSG_ASSIGN;
        ok = n == 7 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_int(t[2], 0, BIG, &m->x0) && !to_int(t[3], 0, BIG, &m->y0) &&
             !to_int(t[4], 0, BIG, &m->x1) && !to_int(t[5], 0, BIG, &m->y1) &&
             !to_int(t[6], 1, MAX_FOLLOWERS, &m->n_followers) && m->x1 >= m->x0 && m->y1 >= m->y0;
    } else if (strcmp(k, "THEME") == 0) {
        m->type = MSG_THEME;
        ok = n == 6 && !to_word(t[1], m->section, sizeof(m->section)) &&
             !to_int(t[2], 0, BIG, &m->start_beat) && !to_int(t[3], 0, BIG, &m->end_beat) &&
             !to_word(t[4], m->theme, sizeof(m->theme)) &&
             !to_word(t[5], m->mood, sizeof(m->mood)) && m->end_beat >= m->start_beat;
    } else if (strcmp(k, "PLAN") == 0) {
        m->type = MSG_PLAN;
        ok = n == 3 && !to_int(t[1], 0, BIG, &m->req_id) && !to_int(t[2], 0, BIG, &m->n);
    } else if (strcmp(k, "BUSY") == 0) {
        m->type = MSG_BUSY;
        ok = n == 3 && !to_int(t[1], 0, BIG, &m->req_id) && !to_int(t[2], 0, 60000, &m->retry_ms);
    } else if (strcmp(k, "BEAT") == 0) {
        m->type = MSG_BEAT;
        ok = n == 2 && !to_int(t[1], 0, BIG, &m->beat);
    } else if (strcmp(k, "SHUTDOWN") == 0) {
        m->type = MSG_SHUTDOWN;
        ok = n == 1;
    } else if (strcmp(k, "HELLO") == 0) {
        m->type = MSG_HELLO;
        ok = n == 3 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_int(t[2], 1, MAX_FOLLOWERS, &m->n_followers);
    } else if (strcmp(k, "PLAN_REQ") == 0) {
        m->type = MSG_PLAN_REQ;
        int seed = 0;
        ok = n == 5 && !to_int(t[1], 0, BIG, &m->req_id) &&
             !to_word(t[2], m->section, sizeof(m->section)) &&
             !to_int(t[3], 1, MAX_FOLLOWERS, &m->n_followers) && !to_int(t[4], 0, INT_MAX, &seed);
        m->style_seed = (unsigned)seed;
    } else if (strcmp(k, "READY") == 0) {
        m->type = MSG_READY;
        ok = n == 3 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_word(t[2], m->section, sizeof(m->section));
    } else if (strcmp(k, "POS") == 0) {
        m->type = MSG_POS;
        ok = n == 7 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_int(t[2], 0, BIG, &m->beat) && !to_int(t[3], 0, MAX_FOLLOWERS - 1, &m->follower) &&
             !to_int(t[4], 0, BIG, &m->x) && !to_int(t[5], 0, BIG, &m->y) &&
             !to_int(t[6], 0, NCOLORS - 1, &m->color);
    } else if (strcmp(k, "ACK") == 0) {
        m->type = MSG_ACK;
        ok = n == 3 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_int(t[2], 0, BIG, &m->beat);
    } else if (strcmp(k, "HEARTBEAT") == 0) {
        m->type = MSG_HEARTBEAT;
        ok = n == 2 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id);
    } else if (strcmp(k, "DONE") == 0) {
        m->type = MSG_DONE;
        ok = n == 3 && !to_int(t[1], 0, MAX_LEADERS - 1, &m->leader_id) &&
             !to_word(t[2], m->section, sizeof(m->section));
    } else {
        m->type = MSG_UNKNOWN;
        return 0;                                   /* unknown names are not an error */
    }
    return ok ? 0 : -1;
}

int proto_format(const Msg *m, char *buf, size_t n) {
    int r;
    switch (m->type) {
    case MSG_ASSIGN:
        r = snprintf(buf, n, "ASSIGN %d %d %d %d %d %d", m->leader_id, m->x0, m->y0, m->x1, m->y1,
                     m->n_followers);
        break;
    case MSG_THEME:
        r = snprintf(buf, n, "THEME %s %d %d %s %s", m->section, m->start_beat, m->end_beat,
                     m->theme, m->mood);
        break;
    case MSG_PLAN:
        r = snprintf(buf, n, "PLAN %d %d", m->req_id, m->n);
        break;
    case MSG_BUSY:
        r = snprintf(buf, n, "BUSY %d %d", m->req_id, m->retry_ms);
        break;
    case MSG_BEAT:
        r = snprintf(buf, n, "BEAT %d", m->beat);
        break;
    case MSG_SHUTDOWN:
        r = snprintf(buf, n, "SHUTDOWN");
        break;
    case MSG_HELLO:
        r = snprintf(buf, n, "HELLO %d %d", m->leader_id, m->n_followers);
        break;
    case MSG_PLAN_REQ:
        r = snprintf(buf, n, "PLAN_REQ %d %s %d %u", m->req_id, m->section, m->n_followers,
                     m->style_seed);
        break;
    case MSG_READY:
        r = snprintf(buf, n, "READY %d %s", m->leader_id, m->section);
        break;
    case MSG_POS:
        r = snprintf(buf, n, "POS %d %d %d %d %d %d", m->leader_id, m->beat, m->follower, m->x, m->y,
                     m->color);
        break;
    case MSG_ACK:
        r = snprintf(buf, n, "ACK %d %d", m->leader_id, m->beat);
        break;
    case MSG_HEARTBEAT:
        r = snprintf(buf, n, "HEARTBEAT %d", m->leader_id);
        break;
    case MSG_DONE:
        r = snprintf(buf, n, "DONE %d %s", m->leader_id, m->section);
        break;
    default:
        return -1;
    }
    return (r < 0 || (size_t)r >= n) ? -1 : r;
}

int proto_parse_keyframe(const char *line, Keyframe *k) {
    char copy[MAX_LINE];
    if (strlen(line) >= sizeof(copy)) {
        return -1;
    }
    strcpy(copy, line);
    char *t[MAX_TOK + 1];
    int n = split(copy, t, MAX_TOK + 1);
    int c;
    if (n != 5 || to_int(t[0], 0, 1 << 20, &k->beat) || to_int(t[1], 0, MAX_FOLLOWERS - 1, &k->follower) ||
        to_int(t[2], -(1 << 20), 1 << 20, &k->x) || to_int(t[3], -(1 << 20), 1 << 20, &k->y) ||
        to_int(t[4], 0, NCOLORS - 1, &c)) {
        return -1;
    }
    k->color = (unsigned char)c;
    return 0;
}

int proto_format_keyframe(const Keyframe *k, char *buf, size_t n) {
    int r = snprintf(buf, n, "%d %d %d %d %d", k->beat, k->follower, k->x, k->y, k->color);
    return (r < 0 || (size_t)r >= n) ? -1 : r;
}
