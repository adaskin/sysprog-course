/* gen_data.c — generates the Task 2 benchmark input (given).
 *
 *   ./gen_data <outfile> <size_in_MB>
 *
 * Deterministic pseudo-random text: words of 1-12 letters separated by one or more
 * spaces, tabs, or newlines, with an occasional very long line. Prints the word count
 * it wrote, so you can compare with `wc -w`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <outfile> <size_in_MB>\n", argv[0]);
        return 1;
    }
    long mb = strtol(argv[2], NULL, 10);
    if (mb < 1 || mb > 4096) {
        fprintf(stderr, "size must be 1..4096 MB\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    unsigned long long state = 88172645463325252ULL;   /* xorshift64, fixed seed */
#define NEXT() (state ^= state << 13, state ^= state >> 7, state ^= state << 17, state)

    long long target = (long long)mb * 1024 * 1024;
    long long written = 0, words = 0;
    int since_newline = 0;
    int long_line_mode = 0;

    while (written < target - 16) {
        int len = 1 + (int)(NEXT() % 12);
        for (int i = 0; i < len; i++) {
            fputc('a' + (int)(NEXT() % 26), f);
        }
        written += len;
        words++;
        since_newline++;

        unsigned r = (unsigned)(NEXT() % 100);
        if (long_line_mode) {
            fputc(' ', f);
            if (since_newline > 200000) {      /* end a huge line */
                fputc('\n', f);
                written++;
                since_newline = 0;
                long_line_mode = 0;
            }
        } else if (r < 70) {
            fputc(' ', f);
        } else if (r < 80) {
            fputc('\t', f);
        } else if (r < 90) {
            fputs("  ", f);
            written++;
        } else {
            fputc('\n', f);
            since_newline = 0;
            if (NEXT() % 5000 == 0) {
                long_line_mode = 1;
            }
        }
        written++;
    }
    fputc('\n', f);
    fclose(f);
    printf("%lld words written to %s\n", words, argv[1]);
    return 0;
}
