#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: wordcount <file>\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "r");

    if (!f) {
        perror("fopen failed");
        return 1;
    }

    int c;
    int words = 0;
    int lines = 0;
    int in_word = 0;

    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') {
            lines++;
        }
        if (c == ' ' || c == '\n' || c == '\t') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }

    fclose(f);

    printf("words: %d\n", words);
    printf("lines: %d\n", lines);

    return 0;
}
