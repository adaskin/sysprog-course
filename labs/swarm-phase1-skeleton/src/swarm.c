/* swarm.c — score and image loaders, zones, logging (given). */
#include "swarm.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void log_msg(const char *who, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[%s] %s\n", who, buf);
}

/* ---- score ---------------------------------------------------------------- */

int score_load(const char *path, Score *s) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->stage_w = 64;
    s->stage_h = 36;
    s->tempo_bpm = 120;

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }
        char kw[16];
        int off = 0;
        if (sscanf(line, "%15s%n", kw, &off) != 1) {
            continue;                                   /* blank line */
        }
        const char *rest = line + off;
        if (strcmp(kw, "stage") == 0) {
            if (sscanf(rest, "%d %d", &s->stage_w, &s->stage_h) != 2 ||
                s->stage_w < 8 || s->stage_h < 4 || s->stage_w > 512 || s->stage_h > 512) {
                goto bad;
            }
        } else if (strcmp(kw, "tempo") == 0) {
            if (sscanf(rest, "%d", &s->tempo_bpm) != 1 || s->tempo_bpm < 0 || s->tempo_bpm > 600) {
                goto bad;
            }
        } else if (strcmp(kw, "image") == 0) {
            if (sscanf(rest, "%127s", s->image_path) != 1) {
                goto bad;
            }
        } else if (strcmp(kw, "section") == 0) {
            if (s->n_sections >= MAX_SECTIONS) {
                goto bad;
            }
            Theme *t = &s->sections[s->n_sections];
            if (sscanf(rest, "%15s %d %d %15s %15s", t->name, &t->start_beat, &t->end_beat,
                       t->theme, t->mood) != 5 ||
                t->start_beat < 0 || t->end_beat < t->start_beat ||
                t->end_beat - t->start_beat + 1 > MAX_BEATS) {
                goto bad;
            }
            if (s->n_sections > 0 && t->start_beat != s->sections[s->n_sections - 1].end_beat + 1) {
                fprintf(stderr, "%s:%d: sections must be contiguous\n", path, lineno);
                fclose(f);
                return -1;
            }
            s->n_sections++;
        } else {
            goto bad;
        }
        continue;
    bad:
        fprintf(stderr, "%s:%d: bad line\n", path, lineno);
        fclose(f);
        return -1;
    }
    fclose(f);
    if (s->n_sections == 0) {
        fprintf(stderr, "%s: no sections\n", path);
        return -1;
    }
    return 0;
}

int score_section_at(const Score *s, int beat) {
    for (int i = 0; i < s->n_sections; i++) {
        if (beat >= s->sections[i].start_beat && beat <= s->sections[i].end_beat) {
            return i;
        }
    }
    return -1;
}

int score_last_beat(const Score *s) {
    return s->sections[s->n_sections - 1].end_beat;
}

/* ---- image (ASCII PGM, P2) ------------------------------------------------- */

static int next_token(FILE *f, char *tok, size_t n) {
    int c;
    do {
        c = fgetc(f);
        if (c == '#') {
            while (c != '\n' && c != EOF) {
                c = fgetc(f);
            }
        }
    } while (c != EOF && isspace(c));
    if (c == EOF) {
        return -1;
    }
    size_t i = 0;
    while (c != EOF && !isspace(c) && i + 1 < n) {
        tok[i++] = (char)c;
        c = fgetc(f);
    }
    tok[i] = '\0';
    return 0;
}

int image_load(const char *path, Image *img) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }
    char tok[32];
    int w, h, maxv;
    if (next_token(f, tok, sizeof(tok)) || strcmp(tok, "P2") != 0 ||
        next_token(f, tok, sizeof(tok)) || (w = atoi(tok)) < 1 || w > 512 ||
        next_token(f, tok, sizeof(tok)) || (h = atoi(tok)) < 1 || h > 512 ||
        next_token(f, tok, sizeof(tok)) || (maxv = atoi(tok)) < 1 || maxv > NCOLORS - 1) {
        fprintf(stderr, "%s: not a P2 PGM with maxval <= 15\n", path);
        fclose(f);
        return -1;
    }
    img->w = w;
    img->h = h;
    img->px = calloc((size_t)w * (size_t)h, 1);
    if (!img->px) {
        fclose(f);
        return -1;
    }
    for (int i = 0; i < w * h; i++) {
        int v;
        if (next_token(f, tok, sizeof(tok)) || (v = atoi(tok)) < 0 || v > maxv) {
            fprintf(stderr, "%s: bad pixel %d\n", path, i);
            free(img->px);
            img->px = NULL;
            fclose(f);
            return -1;
        }
        img->px[i] = (unsigned char)v;
    }
    fclose(f);
    return 0;
}

void image_free(Image *img) {
    free(img->px);
    img->px = NULL;
}

/* ---- zones ------------------------------------------------------------------ */

void zone_split(int stage_w, int stage_h, int n, int i, Zone *z) {
    z->x0 = stage_w * i / n;
    z->x1 = stage_w * (i + 1) / n - 1;
    z->y0 = 0;
    z->y1 = stage_h - 1;
}

int zone_contains(const Zone *z, int x, int y) {
    return x >= z->x0 && x <= z->x1 && y >= z->y0 && y <= z->y1;
}

void home_position(const Zone *z, int i, int *x, int *y) {
    int zw = z->x1 - z->x0 + 1;
    *x = z->x0 + i % zw;
    *y = z->y1 - i / zw;
}
