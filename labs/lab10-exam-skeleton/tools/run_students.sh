#!/bin/sh
# usage: sh tools/run_students.sh N [PORT] [THINK_MS] [FIRST_INDEX]
# Starts N student programs (bin/student) AT THE SAME TIME against the exam server
# on 127.0.0.1:PORT, waits for all of them, and prints how many succeeded and how
# long it took. Student i uses id 1001+i and its pin from data/students.csv.
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
OK=$(ls "$OUT"/*.ok 2>/dev/null | wc -l)
SECS=$(awk -v a=$START -v b=$END 'BEGIN{printf "%.2f", b-a}')
echo "students: $OK/$N ok in ${SECS}s"
if [ "$OK" -ne "$N" ]; then
    for f in $(ls "$OUT"/*.err | head -3); do [ -s "$f" ] && echo "  e.g. $(basename $f .err): $(head -1 $f)"; done
fi
[ -n "${SHOW:-}" ] && cat "$OUT"/*.out | tail -n 5
rm -rf "$OUT"
[ "$OK" -eq "$N" ]
