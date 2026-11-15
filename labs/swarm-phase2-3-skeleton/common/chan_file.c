/* chan_file.c — the flight recorder: a channel into a memory-mapped file (Task 4: YOUR CODE).
 *
 * Address "file:PATH". The file layout is in recording.h: a 64-byte header, then the messages
 * (Frames) back to back. Every message has the same size, sizeof(Frame); refuse others.
 *
 *   CHAN_SEND (the center records the show):
 *     - open(O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0644)
 *     - size the file for the header and REC_MAX_FRAMES frames (ftruncate), then
 *       mmap(PROT_READ | PROT_WRITE, MAP_SHARED): writing a frame is now a memcpy, and the kernel
 *       writes the pages to the file. (A mapping does not grow with the file: what happens if you
 *       touch a page past the end of the file?)
 *     - fill in the header: magic, version, frame_size, n_frames = 0, complete = 0, tempo_bpm
 *       (from the first frame), score name (optional)
 *     - send: copy the frame into slot n_frames, THEN increment n_frames (why in this order?
 *       think of a viewer replaying the file while it is being written); CHAN_TOOBIG when full
 *     - close: complete = 1, msync, munmap, ftruncate the file down to the frames actually
 *       written, close
 *   CHAN_RECV (a viewer replays it):
 *     - open(O_RDONLY), fstat, mmap(PROT_READ, MAP_SHARED)
 *     - check the magic, version and frame_size, and that the file is as long as n_frames says;
 *       refuse (NULL, with a message) anything else: a truncated or foreign file must not crash
 *       the viewer
 *     - recv: the next frame, in order (read n_frames again every time: the file may still be
 *       growing). No next frame yet: CHAN_CLOSED if complete is set, otherwise CHAN_TIMEOUT
 *       (the show is still being recorded; timeout_ms is not used, the viewer polls)
 *     - close: munmap, close
 *   Optional: a center that crashed never sets complete, so its replay would wait forever.
 *   Notice it: the recorder holds flock(fd, LOCK_EX) while it writes; when the replay has no next
 *   frame it tries flock(fd, LOCK_SH | LOCK_NB): if it gets the lock, nobody is writing any more.
 */
#define _GNU_SOURCE
#include "chan_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include "recording.h"

#define REC_MAX_FRAMES 1024

struct file_chan {
    struct chan base;           /* must be first */
    /* TODO: the descriptor, the mapping and its size, the header, the next frame to read, ... */
};

static int file_send(chan_t *c, const void *msg, size_t len) {
    (void)c;
    (void)msg;
    (void)len;
    return CHAN_CLOSED;         /* TODO (Task 4) */
}

static ssize_t file_recv(chan_t *c, void *buf, size_t cap, int timeout_ms) {
    (void)c;
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    return CHAN_CLOSED;         /* TODO (Task 4) */
}

static void file_close(chan_t *c) {
    free(c);                    /* TODO (Task 4) */
}

static const struct chan_ops file_ops = { .send = file_send, .recv = file_recv, .close = file_close };

chan_t *chan_file_open(const char *path, int mode) {
    (void)path;
    (void)mode;
    (void)file_ops;
    fprintf(stderr, "chan_file: not implemented yet (Task 4)\n");
    return NULL;                /* TODO (Task 4) */
}
