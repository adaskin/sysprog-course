/* test_protocol.c — checks of the given protocol code (given). */
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "line_io.h"
#include <unistd.h>

static int failures;
#define CHECK_TRUE(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void roundtrip(const char *line) {
    Msg m;
    char buf[MAX_LINE];
    CHECK_TRUE(proto_parse(line, &m) == 0);
    CHECK_TRUE(m.type != MSG_UNKNOWN);
    CHECK_TRUE(proto_format(&m, buf, sizeof(buf)) > 0);
    if (strcmp(buf, line) != 0) {
        printf("FAIL roundtrip: '%s' -> '%s'\n", line, buf);
        failures++;
    }
}

static int rejects(const char *line) {
    Msg m;
    return proto_parse(line, &m) == -1;
}

int main(void) {
    roundtrip("ASSIGN 2 32 0 47 35 16");
    roundtrip("THEME intro 0 15 dawn calm");
    roundtrip("PLAN 7 512");
    roundtrip("BUSY 7 50");
    roundtrip("BEAT 12");
    roundtrip("SHUTDOWN");
    roundtrip("HELLO 1 16");
    roundtrip("PLAN_REQ 3 march 16 12345");
    roundtrip("READY 0 intro");
    roundtrip("POS 1 20 5 33 17 9");
    roundtrip("ACK 1 20");
    roundtrip("HEARTBEAT 3");
    roundtrip("DONE 0 finale");

    Msg m;
    CHECK_TRUE(proto_parse("FLY AWAY 1 2", &m) == 0 && m.type == MSG_UNKNOWN);
    CHECK_TRUE(rejects("BEAT"));
    CHECK_TRUE(rejects("BEAT x"));
    CHECK_TRUE(rejects("BEAT 1 2"));
    CHECK_TRUE(rejects("BEAT -4"));
    CHECK_TRUE(rejects("BEAT 12abc"));
    CHECK_TRUE(rejects("POS 1 20 5 33 17 99"));            /* color out of range */
    CHECK_TRUE(rejects("ASSIGN 0 10 0 5 35 16"));          /* x1 < x0 */
    CHECK_TRUE(rejects("HELLO 99 16"));                    /* leader id out of range */
    CHECK_TRUE(rejects("THEME intro 20 10 dawn calm"));    /* end < start */
    CHECK_TRUE(rejects(""));

    char longline[600];
    memset(longline, 'A', sizeof(longline) - 1);
    longline[sizeof(longline) - 1] = '\0';
    CHECK_TRUE(rejects(longline));

    Keyframe k;
    char buf[MAX_LINE];
    CHECK_TRUE(proto_parse_keyframe("12 3 40 17 5", &k) == 0 && k.beat == 12 && k.follower == 3 &&
               k.x == 40 && k.y == 17 && k.color == 5);
    CHECK_TRUE(proto_format_keyframe(&k, buf, sizeof(buf)) > 0 && strcmp(buf, "12 3 40 17 5") == 0);
    CHECK_TRUE(proto_parse_keyframe("12 3 40 17", &k) == -1);
    CHECK_TRUE(proto_parse_keyframe("12 3 40 17 16", &k) == -1);

    /* line framing: byte-at-a-time writes, several lines in one write, over-long line */
    int fds[2];
    CHECK_TRUE(pipe(fds) == 0);
    LineReader lr;
    lr_init(&lr, fds[0]);
    char out[32];
    CHECK_TRUE(write(fds[1], "BE", 2) == 2);
    CHECK_TRUE(write(fds[1], "AT 1\nBEAT 2\n", 12) == 12);
    CHECK_TRUE(lr_read_line(&lr, out, sizeof(out)) == 1 && strcmp(out, "BEAT 1") == 0);
    CHECK_TRUE(lr_read_line(&lr, out, sizeof(out)) == 1 && strcmp(out, "BEAT 2") == 0);
    CHECK_TRUE(write(fds[1], "0123456789012345678901234567890123456789\nOK\n", 44) == 44);
    CHECK_TRUE(lr_read_line(&lr, out, sizeof(out)) == -2);
    CHECK_TRUE(lr_read_line(&lr, out, sizeof(out)) == 1 && strcmp(out, "OK") == 0);
    close(fds[1]);
    CHECK_TRUE(lr_read_line(&lr, out, sizeof(out)) == 0);
    close(fds[0]);

    if (failures) {
        printf("test_protocol: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_protocol: all passed\n");
    return 0;
}
