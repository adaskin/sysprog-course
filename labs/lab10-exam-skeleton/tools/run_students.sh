#!/bin/sh
# GIVEN: start N student clients at the same time and report success/time.
N=${1:?usage: run_students.sh N [PORT] [THINK_MS] [FIRST_INDEX]}
PORT=${2:-5200}
THINK=${3:-100}
FIRST=${4:-0}
mkdir -p results
OUT=$(mktemp -d)
START=$(date +%s.%N)
i=$FIRST
while [ "$i" -lt $((FIRST + N)) ]; do
    ID=$((1001 + i))
    PIN=$(awk -F, -v id=$ID '$1==id {print $3}' data/students.csv)
    ( bin/student 127.0.0.1 "$PORT" "$ID" "$PIN" "$THINK" > "$OUT/$ID.out" 2> "$OUT/$ID.err" \
        && echo ok > "$OUT/$ID.ok" ) &
    i=$((i + 1))
done
wait
END=$(date +%s.%N)
OK=$(find "$OUT" -name '*.ok' | wc -l)
SECS=$(awk -v a="$START" -v b="$END" 'BEGIN{printf "%.2f", b-a}')
echo "students: $OK/$N ok in ${SECS}s"
if [ "$OK" -ne "$N" ]; then
    find "$OUT" -name '*.err' | head -3 | while read -r f; do
        [ -s "$f" ] && echo "  e.g. $(basename "$f" .err): $(head -1 "$f")"
    done
fi
[ -n "${SHOW:-}" ] && cat "$OUT"/*.out | tail -n 5
rm -rf "$OUT"
[ "$OK" -eq "$N" ]
