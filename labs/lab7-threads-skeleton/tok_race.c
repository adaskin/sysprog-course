/* tok_race.c — Task 4 (YOUR CODE).
 *
 *   ./tok_race            use strtok in both threads (should fail sometimes)
 *   ./tok_race --safe     use strtok_r (must never fail)
 *
 * Two threads each tokenize their own, different string many times and compare their token count
 * with the expected one. Print "ok" or "MISMATCH after <n> rounds" and exit with status 0 / 1.
 */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

#define ROUNDS 200000

/* Two different strings with different token counts. strtok modifies its input, so each round
 * works on a fresh copy. */
static const char *TEXT[2]     = { "alpha beta gamma delta epsilon", "one two three" };
static const int   EXPECTED[2] = { 5, 3 };

static int safe = 0;

/* TODO: the thread function. arg points to an int, 0 or 1 (which string). For ROUNDS rounds: copy
 * the string into a local buffer, count tokens with strtok (or strtok_r if `safe`), and compare. On a
 * mismatch, remember it. Return a pointer-sized flag (0 ok, 1 mismatch) through the return value. */
static void *tokenizer(void *arg) {
    (void)arg;
    (void)TEXT;
    (void)EXPECTED;
    (void)safe;
    return NULL;
}

int main(int argc, char **argv) {
    safe = argc > 1 && strcmp(argv[1], "--safe") == 0;
    (void)tokenizer;                /* remove once you use it */
    /* TODO: start the two threads (pass each its own index), join, report, set the exit status. */
    fprintf(stderr, "tok_race: not implemented yet\n");
    return 1;                       /* replace with your real exit status */
}
