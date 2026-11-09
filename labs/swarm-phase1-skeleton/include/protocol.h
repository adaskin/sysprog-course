/* protocol.h — the line protocol (given). One message per line, fields separated by single
 * spaces, at most MAX_LINE-1 characters. See the project spec, Section 3.
 *
 * Unknown message names parse as MSG_UNKNOWN (log and ignore them). Malformed known messages
 * (wrong field count, non-numbers, out-of-range values) are rejected with -1. Never crash. */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "swarm.h"

typedef enum {
    MSG_UNKNOWN = 0,
    /* center -> leader */
    MSG_ASSIGN,       /* ASSIGN <leader_id> <x0> <y0> <x1> <y1> <n_followers> */
    MSG_THEME,        /* THEME <section> <start_beat> <end_beat> <theme> <mood> */
    MSG_PLAN,         /* PLAN <req_id> <n>   (followed by n keyframe lines) */
    MSG_BUSY,         /* BUSY <req_id> <retry_ms> */
    MSG_BEAT,         /* BEAT <n> */
    MSG_SHUTDOWN,     /* SHUTDOWN */
    /* leader -> center */
    MSG_HELLO,        /* HELLO <leader_id> <n_followers> */
    MSG_PLAN_REQ,     /* PLAN_REQ <req_id> <section> <n_followers> <style_seed> */
    MSG_READY,        /* READY <leader_id> <section> */
    MSG_POS,          /* POS <leader_id> <beat> <follower> <x> <y> <color> */
    MSG_ACK,          /* ACK <leader_id> <beat>   (all POS lines of this beat were sent) */
    MSG_HEARTBEAT,    /* HEARTBEAT <leader_id> */
    MSG_DONE          /* DONE <leader_id> <section> */
} MsgType;

typedef struct {
    MsgType  type;
    int      leader_id, req_id, beat, n, retry_ms;
    int      x0, y0, x1, y1, n_followers;
    int      follower, x, y, color;
    int      start_beat, end_beat;
    unsigned style_seed;
    char     section[16], theme[16], mood[16];
} Msg;

int proto_parse(const char *line, Msg *m);                  /* 0 ok, -1 malformed */
int proto_format(const Msg *m, char *buf, size_t n);        /* length written, or -1 */

/* Keyframe lines inside a PLAN: "<beat> <follower> <x> <y> <color>" */
int proto_parse_keyframe(const char *line, Keyframe *k);    /* 0 ok, -1 malformed */
int proto_format_keyframe(const Keyframe *k, char *buf, size_t n);

#endif
