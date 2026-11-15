#!/bin/bash
# tsan_show.sh — `make tsan` (given): the show under ThreadSanitizer, driven by operator commands
# (pause, resume, a bad command, a kill, a tempo change) with a viewer attached, so the control
# thread, the paused clock and the restart all run. Any ThreadSanitizer WARNING fails it.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/logs
n="tsan$$"
log=tests/logs/tsan.log
LEADER_BIN=bin/leader_tsan bin/center_tsan --tempo 1200 --paused --timeout 120 --name "$n" \
    --record "/tmp/$n.rec" >tests/logs/tsan.out 2>"$log" &
cp=$!
for _ in $(seq 50); do bin/swarmctl --name "$n" status >/dev/null 2>&1 && break; sleep 0.2; done
bin/viewer --name "$n" --view check >tests/logs/tsan.view 2>&1 &
ctl() { bin/swarmctl --name "$n" "$@"; }
ctl resume; sleep 0.5; ctl pause; sleep 0.3; ctl raw "BOGUS"; ctl resume; sleep 0.3
ctl kill 1; sleep 0.5; ctl tempo 0
wait $cp
rc=$?
wait
rm -f "/tmp/$n.rec"
grep '^RESULT' tests/logs/tsan.out
w=$(grep -c 'WARNING: ThreadSanitizer' "$log")
if [ "$w" -ne 0 ] || [ $rc -ne 0 ]; then
    echo "tsan: $w WARNING(s), center exit $rc: see $log"
    exit 1
fi
echo "tsan: no WARNING (the commands and a captain restart ran under ThreadSanitizer)"
