#!/usr/bin/env python3
"""render.py: turn the center's ASCII stage (./center --ascii 1) into PNG/GIF pictures.

Used by the instructor to make the pictures in the Phase 1 handout (needs Pillow):

    ./center --score data/scores/show.score --followers 46 --tempo 0 --ascii 1 > frames.txt
    python3 render.py frames.txt --montage 0,12,24,36,55 out.png
    python3 render.py frames.txt --gif show.gif
"""
import argparse
import colorsys
import re
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

BG = (10, 14, 32)


def palette():
    cols = [BG]
    for i in range(1, 15):                      # 1..14: a rainbow
        r, g, b = colorsys.hsv_to_rgb((i - 1) / 14 * 0.85, 0.75, 1.0)
        cols.append((int(r * 255), int(g * 255), int(b * 255)))
    cols.append((255, 255, 255))                # 15: white (the home color)
    return cols


PAL = palette()


def read_frames(path):
    frames, cur, beat = {}, None, None
    for line in open(path):
        line = line.rstrip("\n")
        m = re.match(r"--- beat (\d+) ---", line)
        if m:
            beat, cur = int(m.group(1)), []
            frames[beat] = cur
        elif cur is not None and re.fullmatch(r"[.0-9a-f]+", line):
            cur.append(line)
    return {b: f for b, f in frames.items() if f}


def draw_frame(rows, cell, zones=4, title=None):
    h, w = len(rows), len(rows[0])
    img = Image.new("RGB", (w * cell, h * cell), BG)
    glow = Image.new("RGB", img.size, (0, 0, 0))
    gd, d = ImageDraw.Draw(glow), ImageDraw.Draw(img)
    for z in range(1, zones):                   # zone borders
        x = z * w // zones * cell
        d.line([(x, 0), (x, h * cell)], fill=(30, 40, 80), width=1)
    r = cell * 0.36
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == ".":
                continue
            c = PAL[int(ch, 16)]
            cx, cy = (x + 0.5) * cell, (y + 0.5) * cell
            gd.ellipse([cx - r * 2, cy - r * 2, cx + r * 2, cy + r * 2], fill=tuple(v // 2 for v in c))
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c)
    glow = glow.filter(ImageFilter.GaussianBlur(cell * 0.5))
    out = ImageChops.screen(img, glow)          # the glow is added on top of the dots
    if title:
        ImageDraw.Draw(out).text((8, 6), title, fill=(200, 210, 255), font=font(cell))
    return out


def font(cell):
    for name in ("DejaVuSans.ttf", "DejaVuSansMono.ttf"):
        try:
            return ImageFont.truetype(name, max(12, cell * 2 - 4))
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("frames")
    ap.add_argument("--montage", nargs=2, metavar=("BEATS", "OUT"))
    ap.add_argument("--gif")
    ap.add_argument("--labels", default="", help="comma separated captions for the montage")
    ap.add_argument("--cell", type=int, default=10)
    a = ap.parse_args()
    frames = read_frames(a.frames)
    if a.montage:
        beats = [int(b) for b in a.montage[0].split(",")]
        caps = a.labels.split(",") if a.labels else [""] * len(beats)
        tiles = [draw_frame(frames[b], a.cell, title=f"beat {b}  {c}".strip()) for b, c in zip(beats, caps)]
        tw, th = tiles[0].size
        cols = 2
        rows_n = (len(tiles) + cols - 1) // cols
        gap = 8
        sheet = Image.new("RGB", (cols * tw + (cols + 1) * gap, rows_n * th + (rows_n + 1) * gap), (4, 6, 16))
        for i, t in enumerate(tiles):
            sheet.paste(t, (gap + (i % cols) * (tw + gap), gap + (i // cols) * (th + gap)))
        sheet.save(a.montage[1], optimize=True)
    if a.gif:
        imgs = [draw_frame(frames[b], a.cell, title=f"beat {b}") for b in sorted(frames)]
        imgs = [i.quantize(colors=48, method=Image.Quantize.MEDIANCUT) for i in imgs]
        imgs[0].save(a.gif, save_all=True, append_images=imgs[1:], duration=100, loop=0, optimize=True)


if __name__ == "__main__":
    sys.exit(main())
