/* palette.h — the colors of the show (given; the same as tools/render.py).
 * Color 0 is "dark", 1..14 are a rainbow, 15 is white (the home color). */
#ifndef PALETTE_H
#define PALETTE_H

typedef struct { unsigned char r, g, b; } Rgb;

static inline Rgb palette_color(int c) {
    if (c <= 0) {
        return (Rgb){ 60, 64, 90 };                   /* a dark drone: dim, but visible */
    }
    if (c >= 15) {
        return (Rgb){ 255, 255, 255 };
    }
    /* HSV(h, 0.75, 1.0) with h = (c - 1) / 14 * 0.85 */
    double h = (c - 1) / 14.0 * 0.85 * 6.0, s = 0.75;
    int i = (int)h;
    double f = h - i, p = 1 - s, q = 1 - s * f, t = 1 - s * (1 - f);
    double r, g, b;
    switch (i % 6) {
    case 0:  r = 1; g = t; b = p; break;
    case 1:  r = q; g = 1; b = p; break;
    case 2:  r = p; g = 1; b = t; break;
    case 3:  r = p; g = q; b = 1; break;
    case 4:  r = t; g = p; b = 1; break;
    default: r = 1; g = p; b = q; break;
    }
    return (Rgb){ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255) };
}

static const Rgb PALETTE_BG    = { 10, 14, 32 };      /* the night sky */
static const Rgb PALETTE_LOST  = { 90, 90, 100 };     /* a drone whose captain is dead */
static const Rgb PALETTE_ZONE  = { 30, 40, 80 };      /* zone borders */

#endif
