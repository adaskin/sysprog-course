/* tokencount.c — Task 2 (YOUR CODE): a multithreaded word counter.
 *
 *   ./tokencount <file> <nthreads>
 *
 * Prints the number of whitespace-separated words, like `wc -w`.
 * Threads never write to shared variables: each writes only its own slot.
 */
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

typedef struct {
    const char *begin;      /* first byte of this shard           */
    const char *end;        /* one past the last byte             */
    long        count;      /* words counted in this shard        */
} shard_t;

/* Read the whole file into a malloc'ed buffer. Returns NULL on error; sets *len. (given) */
static char *read_file(const char *path, size_t *len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }
    size_t off = 0;
    while (off < (size_t)st.st_size) {
        ssize_t n = read(fd, buf + off, (size_t)st.st_size - off);
        if (n < 0) {
            perror("read");
            free(buf);
            close(fd);
            return NULL;
        }
        if (n == 0) {
            break;
        }
        off += (size_t)n;
    }
    close(fd);
    buf[off] = '\0';
    *len = off;
    return buf;
}

/* TODO: count words in [s->begin, s->end) and store in s->count.
 * A word is a maximal run of non-whitespace bytes (space, \t, \n, \r, \v, \f).
 * Because shard boundaries are moved to whitespace, a shard always starts at a word start
 * or at whitespace — think about why that makes counting independent per shard. */
static void *count_shard(void *arg) {
    shard_t *s = arg;
    (void)s;
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <file> <nthreads>\n", argv[0]);
        return 1;
    }
    int nthreads = atoi(argv[2]);
    if (nthreads < 1 || nthreads > MAX_AGENTS) {
        fprintf(stderr, "nthreads must be 1..%d\n", MAX_AGENTS);
        return 1;
    }
    size_t len = 0;
    char *buf = read_file(argv[1], &len);
    if (!buf) {
        return 1;
    }

    shard_t shards[MAX_AGENTS];
    pthread_t tid[MAX_AGENTS];
    (void)tid;

    /* TODO: split [buf, buf+len) into nthreads shards of roughly equal size. Move every boundary
     * FORWARD to the next whitespace byte so no word is cut in half. Shards may be empty
     * (tiny or empty files). Then create the threads, join them, and sum the counts. */
    (void)shards;
    (void)count_shard;              /* remove once you use it */

    double t0 = now_sec();
    long total = 0;
    /* TODO: create, join, sum. */
    double t1 = now_sec();

    printf("%ld\n", total);
    fprintf(stderr, "time: %.3f s with %d thread(s)\n", t1 - t0, nthreads);
    free(buf);
    return 0;
}
