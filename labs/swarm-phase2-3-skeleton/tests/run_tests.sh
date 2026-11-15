#!/bin/bash
# run_tests.sh — the checks behind `make test` (given). Run it from the project directory.
# Every check prints PASS or FAIL; the exit status is the number of failures.
# Logs of the failed checks are in tests/logs/.
#
#   tests/run_tests.sh            Phase 2: everything below except Phase 3  (make test)
#   tests/run_tests.sh phase1     only the Phase 1 checks (your migrated code)
#   tests/run_tests.sh bus|ctl|rec|signals|valgrind     one Phase 2 part
#   tests/run_tests.sh phase3     Phase 3: sockets net radio faults vg3      (make test3)
#   tests/run_tests.sh sockets|net|radio|faults|vg3     one Phase 3 part

cd "$(dirname "$0")/.." || exit 1
only=${1:-all}
fails=0
LOGS=tests/logs
mkdir -p "$LOGS"
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1"; [ -n "$2" ] && echo "      $2"; fails=$((fails + 1)); }
P2="phase1 bus ctl rec signals valgrind"
P3="sockets net radio faults vg3"
case $only in
    all|phase2) sel="$P2" ;;
    phase3)     sel="$P3" ;;
    everything) sel="$P2 $P3" ;;
    *)          sel="$only" ;;
esac
want() { case " $sel " in *" $1 "*) return 0 ;; esac; return 1; }

# value of key=... in a RESULT, CHECK or status line
field() { echo "$1" | grep -o "\b$2=[-0-9.]*" | head -1 | cut -d= -f2; }

# every show gets its own name, so the checks never meet each other (or your own demo)
show() { echo "t$$$1"; }
PORTS=""                                             # Phase 3: the ports of our centers
ours() { pgrep -f -- "--center 127.0.0.1:$1( |$)"; } # captains and performers of the show on port $1
cleanup() {
    jobs -p | xargs -r kill -KILL 2>/dev/null
    for p in $PORTS; do ours "$p" | xargs -r kill -KILL 2>/dev/null; done
    for f in /dev/shm/t$$* /tmp/t$$*; do [ -e "$f" ] && rm -f "$f"; done
}
trap cleanup EXIT

# wait_pid PID SEC: waits at most SEC seconds for a background process; its exit status, or 124
wait_pid() {
    for _ in $(seq $(($2 * 10))); do
        if ! kill -0 "$1" 2>/dev/null; then
            wait "$1"
            return $?
        fi
        sleep 0.1
    done
    kill -KILL "$1" 2>/dev/null
    wait "$1" 2>/dev/null
    return 124
}

# cpu_ticks PID: user + system CPU time of a process, in clock ticks (1/100 s)
cpu_ticks() { awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null || echo 0; }

# ready NAME: waits (at most 5 s) until the show NAME publishes frames; prints its status line
ready() {
    for _ in $(seq 25); do
        st=$(bin/swarmctl --name "$1" status 2>/dev/null) && { echo "$st"; return 0; }
        sleep 0.2
    done
    return 1
}

# tcp_port LOG: waits (at most 5 s) for the center's "listening on tcp port P" line; prints P
tcp_port() {
    for _ in $(seq 50); do
        p=$(grep -o 'listening on tcp port [0-9]*' "$1" 2>/dev/null | awk '{print $5}')
        [ -n "$p" ] && { PORTS="$PORTS $p"; echo "$p"; return 0; }
        sleep 0.1
    done
    return 1
}

# ready_tcp PORT: waits (at most 10 s) until a TCP status works; prints the status line
ready_tcp() {
    for _ in $(seq 50); do
        st=$(timeout 5 bin/swarmctl --center "127.0.0.1:$1" status 2>/dev/null) && { echo "$st"; return 0; }
        sleep 0.2
    done
    return 1
}

# run_show LABEL [center args...]: a whole show at tempo 0; checks the safety numbers.
# Leaves the RESULT line in $RES and the exit status in $RC.
run_show() {
    label=$1; shift
    OUT=$(timeout 90 bin/center --tempo 0 --timeout 60 --name "$(show run)" "$@" 2>"$LOGS/show.log")
    RC=$?
    RES=$(echo "$OUT" | grep '^RESULT')
    if [ $RC -eq 3 ] || [ $RC -eq 124 ]; then
        fail "$label" "the show hung (exit $RC): deadlock or a missing wake-up? see $LOGS/show.log"
        return 1
    fi
    if [ -z "$RES" ]; then
        fail "$label" "no RESULT line (exit $RC); stderr: $(head -3 "$LOGS/show.log" | tr '\n' '|')"
        return 1
    fi
    if [ "$(field "$RES" collisions)" != 0 ] || [ "$(field "$RES" speed_violations)" != 0 ] || [ $RC -ne 0 ]; then
        fail "$label" "$RES (exit $RC)"
        return 1
    fi
    return 0
}

if want phase1; then
    echo "== Phase 1: your code in the new layout"
    tests/test_protocol >/dev/null 2>&1 && pass "protocol parser and formatter" || fail "protocol parser and formatter"
    tests/test_planner  >/dev/null 2>&1 && pass "planner and validator"         || fail "planner and validator"
    if timeout 60 tests/test_queue >"$LOGS/queue.log" 2>&1; then pass "queue (order, full, close, wake-up, stress)"
    else fail "queue" "$(grep -m1 FAIL "$LOGS/queue.log" || echo "timeout or crash: see $LOGS/queue.log")"; fi
    if grep -nE '\b(u?sleep|nanosleep)\(' common/queue.c >/dev/null; then fail "queue.c has no sleeping/polling"
    else pass "queue.c has no sleeping/polling"; fi
    if run_show "short score, one lock"; then
        acc=$(field "$RES" mosaic_accuracy)
        awk -v a="$acc" 'BEGIN{exit !(a >= 0.95)}' && pass "short score: 0 collisions, 0 speed violations, accuracy $acc" \
            || fail "short score: mosaic accuracy" "$RES"
    fi
    run_show "per-row locks" --grid row && pass "per-row locks"
    if run_show "BUSY pressure" --workers 1 --queue-cap 1 --think-ms 80; then
        b=$(field "$RES" busy)
        [ "${b:-0}" -ge 1 ] && pass "queue full -> BUSY ($b), the show still finishes" || fail "BUSY pressure" "busy=$b"
    fi
    run_show "flaky planner" --flaky 50 && pass "flaky planner: no bad plan reached the stage"
    if run_show "captain 1 dies at beat 20" --battery 1:20; then
        r=$(field "$RES" restarts)
        [ "$r" = 1 ] && pass "a dead captain is restarted" || fail "captain restart" "$RES"
    fi
    timeout 30 bin/swapstress row 8 2 >"$LOGS/swap.log" 2>&1
    rc=$?
    [ $rc -eq 0 ] && pass "swapstress row (lock order)" || fail "swapstress row" "exit $rc (3: deadlock)"
fi

if want bus; then
    echo "== Task 1: the stage bus (shm)"
    timeout 120 tests/test_chan shm >"$LOGS/chan_shm.log" 2>&1
    grep -E '^(PASS|FAIL|HANG|      )' "$LOGS/chan_shm.log" | sed 's/^\(PASS\|FAIL\|HANG\)  shm: /\1  bus: /'
    fails=$((fails + $(grep -cE '^(FAIL|HANG)' "$LOGS/chan_shm.log")))

    # a live show with a viewer that checks every frame it gets
    n=$(show live)
    bin/viewer --name "$n" --view check --wait 5 >"$LOGS/live.view" 2>&1 &
    vp=$!
    OUT=$(timeout 60 bin/center --name "$n" --tempo 2400 --timeout 30 2>"$LOGS/live.log")
    wait_pid $vp 10
    C=$(grep '^CHECK' "$LOGS/live.view")
    if [ -n "$C" ] && [ "$(field "$C" torn)" = 0 ] && [ "$(field "$C" order_errors)" = 0 ] &&
       [ "$(field "$C" finished)" = 1 ] && [ "$(field "$C" frames)" -ge 10 ]; then
        pass "a live viewer gets the show: $(field "$C" frames) frames, none torn, in order, to the end"
    else
        fail "live viewer" "${C:-no CHECK line: $(head -2 "$LOGS/live.view" | tr '\n' '|')}"
    fi

    # a slow viewer and a frozen one must not slow the show down
    OUT=$(timeout 60 bin/center --name "$(show base)" --tempo 2400 --timeout 30 2>/dev/null)
    e0=$(field "$(echo "$OUT" | grep '^RESULT')" elapsed)
    n=$(show slow)
    bin/viewer --name "$n" --view check --wait 5 --delay-ms 300 >"$LOGS/slow.view" 2>&1 &
    v1=$!
    bin/viewer --name "$n" --view check --wait 5 >"$LOGS/frozen.view" 2>&1 &
    v2=$!
    ( for _ in $(seq 50); do [ -e "/dev/shm/$n" ] && break; sleep 0.1; done; sleep 0.2; kill -STOP $v2 2>/dev/null ) &
    sleep 0.3
    OUT=$(timeout 60 bin/center --name "$n" --tempo 2400 --timeout 30 2>"$LOGS/slow.log")
    rc=$?
    e1=$(field "$(echo "$OUT" | grep '^RESULT')" elapsed)
    kill -CONT $v2 2>/dev/null
    if [ $rc -eq 0 ] && [ -n "$e1" ] && awk -v a="$e1" -v b="${e0:-0}" 'BEGIN{exit !(a < b + 1.0)}'; then
        pass "a slow viewer and a stopped viewer do not slow the show (${e0}s alone, ${e1}s with them)"
    else
        fail "slow viewers" "the show took ${e1:-?}s with a slow and a stopped viewer, ${e0:-?}s alone (exit $rc)"
    fi
    wait_pid $v1 30
    wait_pid $v2 10
    C1=$(grep '^CHECK' "$LOGS/slow.view"); C2=$(grep '^CHECK' "$LOGS/frozen.view")
    if [ "$(field "$C1" finished)" = 1 ] && [ "$(field "$C2" finished)" = 1 ] &&
       [ "$(field "$C1" torn)" = 0 ] && [ "$(field "$C2" torn)" = 0 ]; then
        pass "the slow and the stopped viewer catch up with the end of the show (newest frame wins)"
    else
        fail "slow viewers see the end" "slow: ${C1:-none} | stopped: ${C2:-none}"
    fi
fi

if want ctl; then
    echo "== Task 3: the control channel and the operator's commands"
    timeout 60 tests/test_chan fifo >"$LOGS/chan_fifo.log" 2>&1
    grep -E '^(PASS|FAIL|HANG|      )' "$LOGS/chan_fifo.log"
    fails=$((fails + $(grep -cE '^(FAIL|HANG)' "$LOGS/chan_fifo.log")))

    n=$(show ctl)
    ctl() { bin/swarmctl --name "$n" "$@" 2>>"$LOGS/ctl.swarmctl"; }
    bin/center --name "$n" --paused --tempo 600 --timeout 30 >"$LOGS/ctl.out" 2>"$LOGS/ctl.log" &
    cp=$!
    if s1=$(ready "$n"); then
        sleep 0.5
        s2=$(ctl status)
        if [ "$(field "$s1" paused)" = 1 ] && [ "$(field "$s1" beat)" = "$(field "$s2" beat)" ]; then
            pass "--paused: the show waits at beat $(field "$s2" beat)"
        else fail "--paused" "$s1 / $s2"; fi
        ctl resume; sleep 0.8; s3=$(ctl status)
        if [ "$(field "$s3" paused)" = 0 ] && [ "$(field "$s3" beat)" -gt "$(field "$s2" beat)" ]; then
            pass "RESUME: the beats go on (beat $(field "$s3" beat))"
        else fail "RESUME" "$s3"; fi
        ctl pause; sleep 0.4; s4=$(ctl status); sleep 0.6; s5=$(ctl status)
        if [ "$(field "$s5" paused)" = 1 ] && [ "$(field "$s4" beat)" = "$(field "$s5" beat)" ]; then
            pass "PAUSE: the beats stop (beat $(field "$s5" beat))"
        else fail "PAUSE" "$s4 / $s5"; fi
        t1=$(cpu_ticks $cp); sleep 1; t2=$(cpu_ticks $cp)
        [ $((t2 - t1)) -lt 10 ] && pass "a paused center does not burn the CPU ($(((t2 - t1) * 10)) ms in 1 s)" \
            || fail "paused center CPU" "$(((t2 - t1) * 10)) ms of CPU time in 1 s while paused: busy-waiting?"
        ctl raw "FLY AWAY"; ctl raw "TEMPO -5"; ctl raw "TEMPO 90 91"; ctl raw "KILL 99"; ctl raw "PAUSE now"
        sleep 0.3
        if kill -0 $cp 2>/dev/null && [ "$(field "$(ctl status)" paused)" = 1 ]; then
            pass "bad commands are rejected and change nothing"
        else fail "bad commands" "the center died or changed state; see $LOGS/ctl.log"; fi
        ctl resume; sleep 0.3; ctl kill 1; sleep 0.5; ctl tempo 0
        wait_pid $cp 30
        rc=$?
        RES=$(grep '^RESULT' "$LOGS/ctl.out")
        if [ $rc -eq 0 ] && [ "$(field "$RES" restarts)" -ge 1 ] 2>/dev/null; then
            pass "KILL 1: the captain is restarted; TEMPO 0: the show runs to the end ($RES)"
        else fail "KILL and TEMPO" "exit $rc: ${RES:-no RESULT}"; fi
    else
        fail "the control show starts" "no frames from show $n (Task 1 first); see $LOGS/ctl.log"
        kill -KILL $cp 2>/dev/null
    fi

    n=$(show stop)
    bin/center --name "$n" --tempo 120 --timeout 30 >"$LOGS/stop.out" 2>"$LOGS/stop.log" &
    cp=$!
    if ready "$n" >/dev/null; then
        bin/swarmctl --name "$n" stop
        wait_pid $cp 5
        rc=$?
        [ $rc -eq 0 ] && grep -q '^RESULT' "$LOGS/stop.out" && pass "STOP: a clean early end (exit 0, RESULT)" \
            || fail "STOP" "exit $rc"
    else
        fail "STOP" "the show never started"; kill -KILL $cp 2>/dev/null
    fi
fi

if want rec; then
    echo "== Task 4: the flight recorder (file)"
    timeout 60 tests/test_chan file >"$LOGS/chan_file.log" 2>&1
    grep -E '^(PASS|FAIL|HANG|      )' "$LOGS/chan_file.log"
    fails=$((fails + $(grep -cE '^(FAIL|HANG)' "$LOGS/chan_file.log")))

    n=$(show rec); rec=/tmp/$n.rec
    bin/center --name "$n" --tempo 1200 --timeout 30 --record "$rec" >"$LOGS/rec.out" 2>"$LOGS/rec.log" &
    cp=$!
    for _ in $(seq 50); do [ -s "$rec" ] && break; sleep 0.1; done
    bin/viewer --replay "$rec" --view check >"$LOGS/follow.view" 2>&1 &
    vp=$!
    wait_pid $cp 30
    rc=$?
    wait_pid $vp 10
    beats=$(field "$(grep '^RESULT' "$LOGS/rec.out")" beats)
    C=$(timeout 10 bin/viewer --replay "$rec" --view check 2>&1 | grep '^CHECK')
    if [ $rc -eq 0 ] && [ -n "$beats" ] && [ "$(field "$C" frames)" = $((beats + 2)) ] &&
       [ "$(field "$C" finished)" = 1 ] && [ "$(field "$C" torn)" = 0 ]; then
        pass "the recording holds the whole show: $((beats + 2)) frames (home, $beats beats, the end)"
    else
        fail "recording" "RESULT beats=${beats:-?} (exit $rc), replay: ${C:-no CHECK line}"
    fi
    if [ -n "$beats" ]; then
        size=$(stat -c %s "$rec" 2>/dev/null)
        [ "$size" = $((64 + (beats + 2) * 4184)) ] && pass "the file is exactly header + frames ($size bytes)" \
            || fail "recording size" "${size:-no file}: expected $((64 + (beats + 2) * 4184)) bytes"
    fi
    F=$(grep '^CHECK' "$LOGS/follow.view")
    [ "$(field "$F" finished)" = 1 ] && [ "$(field "$F" torn)" = 0 ] &&
        pass "a replay started during the recording follows it to the end ($(field "$F" frames) frames)" \
        || fail "replay while recording" "${F:-no CHECK line}"
    timeout 10 bin/viewer --replay "$rec" --view ppm --out "$LOGS/last.ppm" >/dev/null 2>&1
    [ "$(head -c 2 "$LOGS/last.ppm" 2>/dev/null)" = P6 ] && pass "the replay draws the last frame as a picture (ppm view)" \
        || fail "ppm view" "no $LOGS/last.ppm"
fi

if want signals; then
    echo "== Ctrl-C and SIGTERM: a clean stop"
    for sig in INT TERM; do
        n=$(show sig$sig)
        bin/center --name "$n" --tempo 120 --timeout 30 >"$LOGS/sig.out" 2>"$LOGS/sig.log" &
        cp=$!
        if ready "$n" >/dev/null; then
            kids=$(pgrep -P $cp | tr '\n' ' ')
            kill -$sig $cp
            wait_pid $cp 5
            rc=$?
            left=""
            [ -e "/dev/shm/$n" ] && left="$left /dev/shm/$n"
            [ -e "/tmp/$n.ctl" ] && left="$left /tmp/$n.ctl"
            for k in $kids; do kill -0 "$k" 2>/dev/null && left="$left captain:$k"; done
            if [ $rc -eq 0 ] && [ -z "$left" ] && grep -q '^RESULT' "$LOGS/sig.out"; then
                pass "SIG$sig: the center stops cleanly (exit 0, RESULT, nothing left behind)"
            else
                fail "SIG$sig" "exit $rc; left behind:${left:- nothing}"
            fi
        else
            fail "SIG$sig" "the show never started"; kill -KILL $cp 2>/dev/null
        fi
    done
fi

if want valgrind; then
    if command -v valgrind >/dev/null; then
        echo "== Valgrind"
        n=$(show vg)
        if valgrind -q --leak-check=full --error-exitcode=9 bin/center --tempo 0 --followers 4 --leaders 2 \
               --timeout 100 --name "$n" --record "/tmp/$n.rec" >/dev/null 2>"$LOGS/vg_center.log"; then
            pass "center (bus, recorder, control channel): no memory errors or leaks"
        else fail "center: memory errors or leaks" "see $LOGS/vg_center.log"; fi
        if valgrind -q --leak-check=full --error-exitcode=9 bin/viewer --replay "/tmp/$n.rec" --view check \
               >/dev/null 2>"$LOGS/vg_viewer.log"; then
            pass "viewer (replay): no memory errors or leaks"
        else fail "viewer: memory errors or leaks" "see $LOGS/vg_viewer.log"; fi
    else
        echo "(valgrind not installed: skipped)"
    fi
fi

# ================================================================================ Phase 3

if want sockets; then
    echo "== Phase 3, Task 1: the tcp and udp transports"
    timeout 120 tests/test_chan net >"$LOGS/chan_net.log" 2>&1
    grep -E '^(PASS|FAIL|HANG|      )' "$LOGS/chan_net.log"
    fails=$((fails + $(grep -cE '^(FAIL|HANG)' "$LOGS/chan_net.log")))
fi

if want net; then
    echo "== Phase 3, Task 2: the center as a TCP server (captains, viewers, operators)"
    if run_show "a show over TCP (the captains connect back)" --port 0; then
        if grep -q "connected from" "$LOGS/show.log"; then
            pass "a show over TCP: every captain connected back, 0 collisions ($(field "$RES" elapsed)s)"
        else fail "a show over TCP" "no captain said HELLO? see $LOGS/show.log"; fi
    fi

    # viewers and operators on a paused show; then the show runs
    n=$(show tcpv)
    bin/center --name "$n" --port 0 --paused --tempo 2400 --timeout 60 >"$LOGS/tcpv.out" 2>"$LOGS/tcpv.log" &
    cp=$!
    if P=$(tcp_port "$LOGS/tcpv.log") && s1=$(ready_tcp "$P"); then
        C="127.0.0.1:$P"
        [ "$(field "$s1" paused)" = 1 ] && pass "swarmctl --center: status over TCP (WATCH) shows the paused show" \
            || fail "status over TCP" "$s1"
        bin/viewer --center "$C" --view check >"$LOGS/tcp_live.view" 2>&1 &
        v0=$!
        bin/viewer --center "$C" --view check --delay-ms 200 >"$LOGS/tcp_slow.view" 2>&1 &
        v1=$!
        bin/viewer --center "$C" --view check >"$LOGS/tcp_frozen.view" 2>&1 &
        v2=$!
        exec 7<>"/dev/tcp/127.0.0.1/$P"              # a caller that never says who it is
        exec 8<>"/dev/tcp/127.0.0.1/$P"; echo "FLY ME TO THE MOON" >&8
        exec 9<>"/dev/tcp/127.0.0.1/$P"; echo "HELLO 7 16 999" >&9      # a captain nobody started
        r1=$(timeout 5 bin/swarmctl --center "$C" raw "FLY AWAY"); e1=$?
        r2=$(timeout 5 bin/swarmctl --center "$C" raw "TEMPO 90 91"); e2=$?
        [ $e1 -ne 0 ] && [ $e2 -ne 0 ] && [ "${r1#ERR}" != "$r1" ] && [ "${r2#ERR}" != "$r2" ] &&
            pass "OPERATE: a bad command is answered ERR (swarmctl exits 1)" || fail "OPERATE bad command" "'$r1' '$r2'"
        sleep 0.5
        kill -STOP $v2
        t0=$(date +%s%N)
        r3=$(timeout 5 bin/swarmctl --center "$C" resume); e3=$?
        [ $e3 -eq 0 ] && [ "$r3" = OK ] && pass "OPERATE: RESUME is answered OK" || fail "OPERATE resume" "'$r3' (exit $e3)"
        wait_pid $cp 60
        rc=$?
        t1=$(date +%s%N)
        exec 7>&- 8>&- 9>&-
        kill -CONT $v2 2>/dev/null
        RES=$(grep '^RESULT' "$LOGS/tcpv.out")
        el=$(field "$RES" elapsed)
        if [ $rc -eq 0 ] && [ -n "$el" ] && awk -v a="$el" 'BEGIN{exit !(a < 6.0)}'; then
            pass "a slow viewer, a stopped viewer and silent or bad callers do not hold the show up (${el}s)"
        else fail "slow and stopped TCP viewers" "exit $rc, ${RES:-no RESULT}; see $LOGS/tcpv.log"; fi
        wait_pid $v0 10; wait_pid $v1 10
        C0=$(grep '^CHECK' "$LOGS/tcp_live.view"); C1=$(grep '^CHECK' "$LOGS/tcp_slow.view")
        if [ "$(field "$C0" finished)" = 1 ] && [ "$(field "$C0" torn)" = 0 ] && [ "$(field "$C0" order_errors)" = 0 ] &&
           [ "$(field "$C0" frames)" -ge 20 ] 2>/dev/null; then
            pass "a TCP viewer gets the show: $(field "$C0" frames) frames, in order, to the end"
        else fail "TCP viewer" "${C0:-no CHECK line: $(head -2 "$LOGS/tcp_live.view" | tr '\n' '|')}"; fi
        if [ "$(field "$C1" finished)" = 1 ] && [ "$(field "$C1" frames)" -lt "$(field "$C0" frames)" ] 2>/dev/null; then
            pass "a slow TCP viewer skips frames ($(field "$C1" frames)) and still sees the end"
        else fail "slow TCP viewer" "slow: ${C1:-none} | normal: ${C0:-none}"; fi
        wait_pid $v2 10
        [ $? -ne 124 ] && pass "the stopped viewer, resumed, ends by itself (it saw the end or was dropped)" \
            || fail "stopped TCP viewer" "still running 10 s after the show"
    else
        fail "the TCP show starts" "no port or no frames (Tasks 1 and 2); see $LOGS/tcpv.log"
        kill -KILL $cp 2>/dev/null
    fi
fi

if want radio; then
    echo "== Phase 3, Task 3: performer processes on the radio (UDP)"
    n=$(show radio)
    bin/center --name "$n" --port 0 --paused --tempo 0 --timeout 60 >"$LOGS/radio.out" 2>"$LOGS/radio.log" &
    cp=$!
    if P=$(tcp_port "$LOGS/radio.log") && ready_tcp "$P" >/dev/null; then
        sleep 1
        np=$(pgrep -f -- "performer .*--center 127.0.0.1:$P( |$)" | wc -l)
        lp=$(pgrep -f -- "leader .*--center 127.0.0.1:$P( |$)" | head -1)
        kids=$(pgrep -P "${lp:-1}" -f performer | wc -l)
        [ "$np" = 64 ] && [ "$kids" = 16 ] && pass "64 performer processes, 16 per captain (its children)" \
            || fail "performer processes" "$np performers running (want 64), captain $lp has $kids (want 16)"
        timeout 5 bin/swarmctl --center "127.0.0.1:$P" resume >/dev/null
        wait_pid $cp 60
        rc=$?
        RES=$(grep '^RESULT' "$LOGS/radio.out")
        sleep 0.3
        left=$(ours "$P" | wc -l)
        if [ "$np" != 64 ]; then
            fail "the show on the radio" "no performers (see above): followers are still threads"
        elif [ $rc -eq 0 ] && [ "$(field "$RES" collisions)" = 0 ] && ! grep -q "late performer" "$LOGS/radio.log"; then
            pass "the show runs on the radio: every GOTO answered by an AT in time ($(field "$RES" elapsed)s)"
            [ "$left" = 0 ] && pass "every performer landed at the end" || fail "performers left after the show" "$left processes"
        else fail "show on the radio" "exit $rc, ${RES:-no RESULT}; see $LOGS/radio.log"; fi
    else
        fail "the radio show starts" "see $LOGS/radio.log"; kill -KILL $cp 2>/dev/null
    fi
    export SWARM_UDP_LOSS=30
    if run_show "30% of the datagrams lost" --port 0; then
        if grep -q "no radio" "$LOGS/show.log"; then
            fail "30% of the datagrams lost" "the captains have no radio (Task 3)"
        else
            pass "30% of the datagrams lost: the show still finishes, 0 collisions ($(field "$RES" elapsed)s)"
        fi
    fi
    unset SWARM_UDP_LOSS
fi

if want faults; then
    echo "== Phase 3, Task 4: heartbeats, orphans, a clean stop"
    if run_show "captain 1 dies at beat 20 (its performers live on)" --port 0 --battery 1:20; then
        if [ "$(field "$RES" restarts)" = 1 ] && [ "$(field "$RES" rehomed)" -ge 16 ] 2>/dev/null; then
            pass "a dead captain's 16 orphans are rehomed to the new one (orphans=$(field "$RES" orphans) rehomed=$(field "$RES" rehomed))"
        else fail "orphan rescue" "$RES (want restarts=1, rehomed>=16)"; fi
    fi
    if run_show "no restart: the orphans land" --port 0 --battery 3:10 --no-restart; then
        [ "$(field "$RES" landed)" = 16 ] && pass "--no-restart: the 16 orphans are told LAND" \
            || fail "orphans land" "$RES (want landed=16)"
    fi

    n=$(show hb)
    bin/center --name "$n" --port 0 --tempo 300 --heartbeat-ms 1500 --timeout 60 >"$LOGS/hb.out" 2>"$LOGS/hb.log" &
    cp=$!
    if P=$(tcp_port "$LOGS/hb.log") && ready_tcp "$P" >/dev/null; then
        lp=$(pgrep -f -- "leader --id 2 .*--center 127.0.0.1:$P( |$)" | head -1)
        kill -STOP "$lp" 2>/dev/null
        sleep 2.5
        grep -q "captain 2 silent" "$LOGS/hb.log" && pass "a stopped captain (SIGSTOP) is declared dead (no HEARTBEAT)" \
            || fail "heartbeat watchdog" "nothing in $LOGS/hb.log 2.5 s after SIGSTOP (--heartbeat-ms 1500)"
        sleep 1; timeout 5 bin/swarmctl --center "127.0.0.1:$P" kill 2 >/dev/null   # the new one dies too
        sleep 2.5; timeout 5 bin/swarmctl --center "127.0.0.1:$P" tempo 0 >/dev/null
        wait_pid $cp 60
        rc=$?
        RES=$(grep '^RESULT' "$LOGS/hb.out")
        if [ $rc -eq 0 ] && [ "$(field "$RES" restarts)" = 2 ] && [ "$(field "$RES" collisions)" = 0 ] &&
           [ "$(field "$RES" rehomed)" -ge 32 ] 2>/dev/null; then
            pass "the adopting captain dies too: the orphans move again, the show ends ($RES)"
        else fail "second death" "exit $rc, ${RES:-no RESULT} (want restarts=2, rehomed>=32)"; fi
        kill -KILL "$lp" 2>/dev/null
    else
        fail "heartbeat show starts" "see $LOGS/hb.log"; kill -KILL $cp 2>/dev/null
    fi

    n=$(show int)
    bin/center --name "$n" --port 0 --tempo 120 --timeout 60 >"$LOGS/int.out" 2>"$LOGS/int.log" &
    cp=$!
    if P=$(tcp_port "$LOGS/int.log") && ready_tcp "$P" >/dev/null; then
        bin/viewer --center "127.0.0.1:$P" --view check >"$LOGS/int.view" 2>&1 &
        vp=$!
        exec 7<>"/dev/tcp/127.0.0.1/$P"; echo OPERATE >&7     # an operator who stays connected
        sleep 1
        before=$(ours "$P" | wc -l)
        t0=$(date +%s%N)
        kill -INT $cp
        wait_pid $cp 5
        rc=$?
        ms=$((($(date +%s%N) - t0) / 1000000))
        exec 7>&-
        wait_pid $vp 5
        vrc=$?
        sleep 0.2
        left=$(ours "$P" | wc -l)
        if [ $rc -eq 0 ] && [ $ms -le 2000 ] && [ "$left" = 0 ] && [ "$before" -gt 60 ] && [ $vrc -ne 124 ] &&
           grep -q '^RESULT' "$LOGS/int.out"; then
            pass "Ctrl-C: all $before processes (captains, performers, a viewer) and an operator gone in $ms ms, exit 0"
        else fail "Ctrl-C over TCP" "exit $rc after $ms ms; $before processes before, $left left; viewer exit $vrc"; fi
    else
        fail "Ctrl-C show starts" "see $LOGS/int.log"; kill -KILL $cp 2>/dev/null
    fi
fi

if want vg3; then
    if command -v valgrind >/dev/null; then
        echo "== Phase 3: valgrind (TCP center, a viewer, an operator, a dead captain)"
        n=$(show vg3)
        valgrind -q --leak-check=full --error-exitcode=9 --track-fds=yes bin/center --port 0 --paused --tempo 600 \
            --battery 2:12 --timeout 100 --name "$n" >"$LOGS/vg3.out" 2>"$LOGS/vg3.log" &
        cp=$!
        if P=$(tcp_port "$LOGS/vg3.log") && ready_tcp "$P" >/dev/null; then
            bin/viewer --center "127.0.0.1:$P" --view check >/dev/null 2>&1 &
            sleep 0.5
            timeout 5 bin/swarmctl --center "127.0.0.1:$P" resume >/dev/null
            wait_pid $cp 100
            rc=$?
            fds=$(grep -o 'FILE DESCRIPTORS: [0-9]* open' "$LOGS/vg3.log" | awk '{print $3}')
            [ $rc -eq 0 ] && pass "TCP center: no memory errors or leaks" || fail "TCP center: valgrind" "exit $rc; see $LOGS/vg3.log"
            [ "${fds:-3}" -le 3 ] && pass "TCP center: no descriptor left open at exit" \
                || fail "TCP center: descriptors left open" "$fds open at exit; see $LOGS/vg3.log"
        else
            fail "valgrind TCP show starts" "see $LOGS/vg3.log"; kill -KILL $cp 2>/dev/null
        fi
    else
        echo "(valgrind not installed: skipped)"
    fi
fi

for p in $PORTS; do
    left=$(ours "$p" | tr '\n' ' ')
    [ -n "$left" ] && fail "no captain or performer of port $p left behind" "$left"
done
leftover=$(ls /dev/shm/t$$* /tmp/t$$*.ctl 2>/dev/null | tr '\n' ' ')
[ -n "$leftover" ] && fail "nothing left behind in /dev/shm or /tmp" "$leftover"
echo
want bus && echo "(Phase 2, Task 2, your GUI view, is shown in the lab: it is not tested here)"
[ $fails -eq 0 ] && echo "ALL CHECKS PASSED" || echo "$fails check(s) FAILED (logs in $LOGS/)"
exit $fails
