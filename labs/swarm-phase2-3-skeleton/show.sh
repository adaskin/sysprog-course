#!/bin/sh
# show.sh — a simple picture view of a show (given). It records the heart show, replays the
# recording through the ascii view and draws show.png (five snapshots) and show.gif (animation)
# with tools/render.py. Needs Pillow (pip install pillow), your Phase 1 code and Task 4 (the
# recorder). Extra options go to the center, e.g.  ./show.sh --battery 1:24
# This is only a picture: your own live window is Task 2 (bin/viewer --view gui).
set -e
make -s
bin/center --score data/scores/show.score --followers 46 --tempo 0 --name "show-$$" \
    --record show.rec "$@" >/dev/null
bin/viewer --replay show.rec --view ascii > frames.txt
python3 tools/render.py frames.txt --montage 0,12,24,36,55 show.png
python3 tools/render.py frames.txt --gif show.gif
echo "wrote show.rec, show.png and show.gif"
