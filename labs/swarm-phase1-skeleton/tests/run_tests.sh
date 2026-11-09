#!/bin/bash
# run_tests.sh — the checks behind `make test` (given). Run it from the project directory.
# Every check prints PASS or FAIL; the exit status is the number of failures.

cd "$(dirname "$0")/.." || exit 1
fails=0
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1"; [ -n "$2" ] && echo "      $2"; fails=$((fails + 1)); }

# value of key=... in a RESULT line
field() { echo "$1" | grep -o "$2=[0-9.]*" | head -1 | cut -d= -f2; }

# run_show NAME [center args...]: runs a whole show at tempo 0 and checks the safety numbers.
# Leaves the RESULT line in $RES and the exit status in $RC.
run_show() {
    name=$1; shift
    OUT=$(timeout 90 ./center --tempo 0 --timeout 60 "$@" 2>stderr.log)
    RC=$?
    RES=$(echo "$OUT" | grep '^RESULT')
    if [ $RC -eq 3 ] || [ $RC -eq 124 ]; then
        fail "$name" "the show hung (exit $RC): deadlock or a missing wake-up? see stderr.log"
        return 1
    fi
    if [ -z "$RES" ]; then
        fail "$name" "no RESULT line (exit $RC); first lines of stderr: $(head -3 stderr.log | tr '\n' '|')"
        return 1
    fi
    if [ "$(field "$RES" collisions)" != 0 ] || [ "$(field "$RES" speed_violations)" != 0 ] || [ $RC -ne 0 ]; then
        fail "$name" "$RES (exit $RC)"
        return 1
    fi
    return 0
}

echo "== unit tests (given modules)"
./tests/test_protocol >/dev/null 2>&1 && pass "protocol parser and formatter" || fail "protocol parser and formatter"
./tests/test_planner  >/dev/null 2>&1 && pass "planner and validator"         || fail "planner and validator"

echo "== Task 1: queue"
if timeout 60 ./tests/test_queue >queue.log 2>&1; then pass "queue behaviour (order, full, close, wake-up, 4x4 stress)"
else fail "queue behaviour" "$(grep -m1 FAIL queue.log || echo 'timeout or crash: see queue.log')"; fi
if grep -nE '\b(u?sleep|nanosleep)\(' src/queue.c >/dev/null; then fail "queue.c has no sleeping/polling" "found sleep in src/queue.c"
else pass "queue.c has no sleeping/polling"; fi
waits=$(grep -c 'pthread_cond_wait' src/queue.c); whiles=$(grep -cE '^\s*while \(.*(count|closed|capacity)' src/queue.c)
if [ "$waits" -gt 0 ] && [ "$whiles" -ge "$waits" ]; then pass "queue.c waits inside while loops"
else fail "queue.c waits inside while loops" "every pthread_cond_wait needs a while (condition) around it"; fi

echo "== Task 2 and 3: the show"
if run_show "short score, one lock" ; then
    acc=$(field "$RES" mosaic_accuracy)
    awk -v a="$acc" 'BEGIN{exit !(a >= 0.95)}' && pass "short score: 0 collisions, 0 speed violations, mosaic accuracy $acc" \
        || fail "short score: mosaic accuracy" "$RES"
fi
run_show "short score, per-row locks" --grid row && pass "short score with per-row locks"
if run_show "BUSY pressure (1 worker, queue of 1, slow planner)" --workers 1 --queue-cap 1 --think-ms 80; then
    b=$(field "$RES" busy)
    [ "${b:-0}" -ge 1 ] && pass "queue full -> BUSY replies ($b) and the show still finishes" \
        || fail "BUSY pressure" "expected at least one BUSY reply, got busy=$b"
fi
if run_show "tiny KV pool (backpressure through the pool)" --pool-blocks 8; then
    b=$(field "$RES" busy)
    [ "${b:-0}" -ge 1 ] && pass "KV pool exhausted -> BUSY replies ($b) and no leak/hang" \
        || fail "tiny KV pool" "expected BUSY replies from an exhausted pool, got busy=$b"
fi
if run_show "flaky planner (50% broken plans)" --flaky 50; then
    pass "flaky planner: invalid plans caught by the validator, no violation reached the stage"
fi

echo "== Task 5: a captain goes dark"
if run_show "captain 1 dies at beat 20, restart" --battery 1:20; then
    r=$(field "$RES" restarts); acc=$(field "$RES" mosaic_accuracy)
    if [ "$r" = 1 ] && awk -v a="$acc" 'BEGIN{exit !(a >= 0.95)}'; then pass "dead captain restarted, the picture still forms ($acc)"
    else fail "captain restart" "$RES"; fi
    if pgrep -x leader >/dev/null; then fail "no zombie or stray leader processes" "$(pgrep -x leader | tr '\n' ' ')still running"
    else pass "no stray leader processes left"; fi
    zombies=$(ps -eo stat,comm | awk '$1 ~ /^Z/ && $2 == "leader"' | wc -l)
    [ "$zombies" -eq 0 ] && pass "no zombie leader processes" || fail "no zombie leader processes" "$zombies zombie(s)"
fi
if run_show "captain 2 dies at beat 40, no restart" --battery 2:40 --no-restart; then
    pass "--no-restart: the show ends without the dead captain (accuracy $(field "$RES" mosaic_accuracy))"
fi

echo "== Task 3 and 4: lock granularity and lock ordering"
if timeout 30 ./swapstress global 8 2 >swap.log 2>&1; then pass "swapstress global (one lock)"; else fail "swapstress global" "$(cat swap.log)"; fi
timeout 30 ./swapstress row 8 2 >swap.log 2>&1
rc=$?
if [ $rc -eq 0 ]; then pass "swapstress row (per-row locks, lock order fixed)"
elif [ $rc -eq 3 ]; then fail "swapstress row" "HANG: grid_swap still deadlocks (Task 4: fix the lock order)"
else fail "swapstress row" "exit $rc: $(cat swap.log)"; fi

if command -v valgrind >/dev/null; then
    echo "== Valgrind"
    if valgrind -q --leak-check=full --error-exitcode=9 ./center --tempo 0 --followers 4 --leaders 2 --timeout 100 >/dev/null 2>vg.log; then
        pass "center: no memory errors or leaks"; rm -f vg.log
    else fail "center: memory errors or leaks" "see vg.log"; fi
else
    echo "(valgrind not installed: skipped)"
fi

rm -f stderr.log queue.log swap.log
echo
[ $fails -eq 0 ] && echo "ALL CHECKS PASSED" || echo "$fails check(s) FAILED"
exit $fails
