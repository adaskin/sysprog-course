/* recording.h — the flight recorder file format (given).
 *
 * A recording is a header followed by Frames, back to back, exactly as they were published:
 *
 *   offset 0                  RecHeader (64 bytes)
 *   offset REC_DATA_OFFSET    Frame 0, Frame 1, ..., Frame n_frames-1     (sizeof(Frame) each)
 *
 * It is the "file:PATH" transport of chan.h (common/chan_file.c, Task 4): the center sends
 * frames into it through a shared, writable file mapping; a viewer receives them back through a
 * read-only mapping. Because every frame has the same size, frame k is at
 * REC_DATA_OFFSET + k * frame_size: a reader can jump to any beat without reading the ones
 * before it.
 *
 * The magic, version and frame_size let a reader refuse a file it does not understand (a
 * different program, or a recording from a build with another Frame layout) instead of
 * showing garbage.
 */
#ifndef RECORDING_H
#define RECORDING_H

#include <stdint.h>
#include "frame.h"

#define REC_MAGIC        "SWARMREC"        /* 8 bytes, no NUL */
#define REC_VERSION      1
#define REC_DATA_OFFSET  64

typedef struct {
    char     magic[8];          /* REC_MAGIC                                                  */
    uint32_t version;           /* REC_VERSION                                                */
    uint32_t frame_size;        /* sizeof(Frame) of the writer                                */
    uint32_t n_frames;          /* frames written so far (updated after each frame)           */
    uint32_t complete;          /* 1 once the recorder was closed properly                     */
    uint32_t tempo_bpm;         /* tempo at the start of the show (for real-time replay)      */
    char     score[36];         /* name of the score file, for information                    */
} RecHeader;

_Static_assert(sizeof(RecHeader) == REC_DATA_OFFSET, "RecHeader must be 64 bytes");

#endif
