#!/bin/sh
# A simple picture view of the show: runs ./center, then draws the ASCII frames as
# show.png (five snapshots) and show.gif (animation) with tools/render.py.
# Needs Pillow:  pip install pillow
# Extra options go to center, e.g.  ./show.sh --battery 1:24
# Works only after Tasks 1-3 are done (the full show needs all of them).
set -e
make -s
./center --score data/scores/show.score --followers 46 --tempo 0 --ascii 1 "$@" > frames.txt
python3 tools/render.py frames.txt --montage 0,12,24,36,55 show.png
python3 tools/render.py frames.txt --gif show.gif
echo "wrote show.png and show.gif"
