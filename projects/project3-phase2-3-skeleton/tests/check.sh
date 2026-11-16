#!/bin/bash
# check.sh — whole shows, tested from the outside (given).
#
#   tests/check.sh phase2 [scenario ...]      fifo:, shm:, file:   (make check2)
#   tests/check.sh phase3 [scenario ...]      tcp:, udp:            (make check3)
#   tests/check.sh list                       the scenarios
#
# Every program is started on its own, as a separate process with no parent link to the others,
# through the command lines of PROTOCOL.md Section 7. In Phase 3 each one gets its own address
# (SWARM_BIND=127.0.0.x), as if it ran on a machine of its own. The script then checks what the
# programs print (RESULT, CHECK, swarmctl answers), what they leave behind (the recording, audited by
# bin/framecheck; nothing in /dev/shm or /tmp), and that every program exits.
#
# Logs: tests/logs/<scenario>/<program>.out and .err. The exit status is the number of failures.

set -u
cd "$(dirname "$0")/.." || exit 2
ROOT=$(pwd)
LOGS=$ROOT/tests/logs
SCORE=data/scores/short.score        # 56 beats
LONG=data/scores/long.score          # 144 beats
PASSES=0
FAILURES=0
declare -A PID=()
SC=""
TAG="swarm-check-$$"

need_programs() {
    local p
    for p in center captain performer viewer swarmctl framecheck; do
        if [ ! -x "bin/$p" ]; then
            echo "bin/$p is missing: run make first (make all bin/framecheck)" >&2
            exit 2
        fi
    done
}

# ------------------------------------------------------------------------------- helpers

check() {                            # check CONDITION-STATUS MESSAGE
    if [ "$1" -eq 0 ]; then
        echo "PASS  [$SC] $2"
        PASSES=$((PASSES + 1))
    else
        echo "FAIL  [$SC] $2"
        FAILURES=$((FAILURES + 1))
    fi
}

# start NAME [VAR=value ...] -- COMMAND ARGS...: in the background, logs in tests/logs/SC/NAME.*
start() {
    local name=$1; shift
    local envs=()
    while [ "$1" != "--" ]; do envs+=("$1"); shift; done
    shift
    env "${envs[@]}" "$@" >"$LOGS/$SC/$name.out" 2>"$LOGS/$SC/$name.err" &
    PID[$name]=$!
}

# finish NAME SECONDS: wait for it to exit; its exit status goes into CODE (124: it did not, and
# was killed). Not in $(...): a subshell cannot wait for this shell's children.
finish() {
    local name=$1 secs=$2 pid=${PID[$1]:-}
    CODE=127
    [ -z "$pid" ] && return
    local end=$((SECONDS + secs))
    while kill -0 "$pid" 2>/dev/null && [ $SECONDS -lt $end ]; do sleep 0.1; done
    if kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
        CODE=124
    else
        wait "$pid" 2>/dev/null
        CODE=$?
    fi
    unset "PID[$name]"
}

# wait_until SECONDS COMMAND...: 0 as soon as COMMAND succeeds
wait_until() {
    local end=$((SECONDS + $1)); shift
    while [ $SECONDS -lt $end ]; do
        "$@" >/dev/null 2>&1 && return 0
        sleep 0.1
    done
    return 1
}

# value KEY FILE: the value of KEY=... on the last RESULT/CHECK/FRAMECHECK/OK line of FILE
value() {
    grep -E '^(RESULT|CHECK|FRAMECHECK|OK) ' "$2" 2>/dev/null | tail -1 | tr ' ' '\n' | sed -n "s/^$1=//p"
}

ctl() {                              # swarmctl, from its own address in Phase 3
    if [ -n "${CTL_BIND:-}" ]; then
        SWARM_BIND=$CTL_BIND bin/swarmctl --center "$CENTER" "$@" 2>>"$LOGS/$SC/swarmctl.err"
    else
        bin/swarmctl --center "$CENTER" "$@" 2>>"$LOGS/$SC/swarmctl.err"
    fi
}

beat_now() { ctl status | tr ' ' '\n' | sed -n 's/^beat=//p'; }
beat_at_least() { local b; b=$(beat_now); [ -n "$b" ] && [ "$b" -ge "$1" ]; }
status_has() { ctl status | grep -q " $1=$2"; }

kill_all() {
    for name in "${!PID[@]}"; do
        kill -9 "${PID[$name]}" 2>/dev/null
        wait "${PID[$name]}" 2>/dev/null
    done
    PID=()
}

cleanup() {
    kill_all
    rm -f /tmp/$TAG-*.fifo* /dev/shm/$TAG-* 2>/dev/null
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM

begin() {
    kill_all
    SC=$1
    mkdir -p "$LOGS/$SC"
    rm -f "$LOGS/$SC"/*
    REC=$LOGS/$SC/show.rec
    echo "---- $SC"
}

# the checks every show ends with
expect_result() {                    # expect_result KEY=VALUE ...
    local out=$LOGS/$SC/center.out
    for kv in "$@"; do
        local k=${kv%%=*} want=${kv#*=} got
        got=$(value "$k" "$out")
        case $want in
            '>'*) [ -n "$got" ] && [ "$got" -gt "${want#>}" ]; check $? "RESULT $k=$got (want more than ${want#>})" ;;
            '<'*) [ -n "$got" ] && [ "$got" -lt "${want#<}" ]; check $? "RESULT $k=$got (want less than ${want#<})" ;;
            *)    [ "$got" = "$want" ]; check $? "RESULT $k=$got (want $want)" ;;
        esac
    done
}

audit() {
    bin/framecheck "$REC" >"$LOGS/$SC/framecheck.out" 2>"$LOGS/$SC/framecheck.err"
    check $? "framecheck: $(cat "$LOGS/$SC/framecheck.out")"
}

all_exit() {                         # every program still running must exit by itself
    for name in "${!PID[@]}"; do
        finish "$name" "$1"
        [ "$CODE" != 124 ]; check $? "$name exits by itself after the show (status $CODE)"
    done
}

nothing_left() {
    local left
    left=$(ls /dev/shm/$TAG-* /tmp/$TAG-*.fifo* 2>/dev/null)
    [ -z "$left" ]; check $? "nothing left in /dev/shm or /tmp ${left:+($left)}"
}

# =============================================================================== Phase 2

p2_setup() {                         # p2_setup EXTRA-CENTER-ARGS...
    FIFO=/tmp/$TAG-$SC.fifo
    BUS=$TAG-$SC
    CENTER=fifo:$FIFO
    CTL_BIND=""
    start center -- bin/center --listen "$CENTER" --bus "shm:$BUS" --record "file:$REC" --wait 20 "$@"
    wait_until 5 test -p "$FIFO" && wait_until 5 test -e "/dev/shm/$BUS"
    local up=$?
    check $up "the center opens fifo:$FIFO and shm:$BUS when it starts"
    return $up
}

p2_basic() {
    begin p2_basic
    p2_setup --score $SCORE --captains 2 --followers 8 --tempo 0 || return
    start view_shm -- bin/viewer --from "shm:$BUS" --check
    start view_local -- bin/viewer --from "$CENTER" --check
    sleep 0.3
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "the center ends the show and exits 0 (status $code)"
    expect_result beats=56 drones=16 collisions=0 speed_violations=0 dead=0
    audit
    for v in view_shm view_local; do
        finish $v 10; code=$CODE
        [ "$code" = 0 ] && [ "$(value finished "$LOGS/$SC/$v.out")" = 1 ] && [ "$(value torn "$LOGS/$SC/$v.out")" = 0 ]
        check $? "viewer --check on ${v#view_}: $(grep '^CHECK' "$LOGS/$SC/$v.out")"
    done
    bin/viewer --from "file:$REC" --check >"$LOGS/$SC/replay.out" 2>"$LOGS/$SC/replay.err"
    code=$?
    local frames
    frames=$(value frames "$LOGS/$SC/framecheck.out")
    [ "$code" = 0 ] && [ "$(value frames "$LOGS/$SC/replay.out")" = "$frames" ]
    check $? "a replay of the recording sees all $frames frames: $(cat "$LOGS/$SC/replay.out")"
    all_exit 5
    nothing_left
}

p2_operator() {
    begin p2_operator
    p2_setup --score $SCORE --captains 2 --followers 8 --tempo 240 || return
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    wait_until 10 beat_at_least 2
    check $? "swarmctl status shows the beat ($(ctl status))"
    local out code b1 b2 b3
    out=$(ctl pause); code=$?
    [ $code = 0 ] && [ "${out:0:2}" = OK ]; check $? "swarmctl pause answers OK ($out)"
    sleep 0.5; b1=$(beat_now); sleep 1; b2=$(beat_now)
    [ -n "$b1" ] && [ "$b1" = "$b2" ]; check $? "paused: the beat stays at $b1 ($b2 a second later)"
    out=$(ctl resume); code=$?
    sleep 1; b3=$(beat_now)
    [ $code = 0 ] && [ -n "$b3" ] && [ "$b3" -gt "$b2" ]; check $? "resumed: the beat moves on ($b2 -> $b3)"
    out=$(ctl frobnicate); code=$?
    [ $code = 1 ] && [ "${out:0:3}" = ERR ]; check $? "an unknown command answers ERR and swarmctl exits 1 ($out)"
    out=$(ctl tempo 0); code=$?
    [ $code = 0 ]; check $? "swarmctl tempo 0 answers OK ($out)"
    finish center 30; code=$CODE
    [ "$code" = 0 ]; check $? "the show runs to its end (status $code)"
    expect_result beats=56 collisions=0 speed_violations=0
    audit
    bin/swarmctl --center "$CENTER" status >/dev/null 2>&1; code=$?
    [ $code = 2 ]; check $? "swarmctl with no center exits 2 ($code)"
    all_exit 5
    nothing_left
}

p2_stop() {
    begin p2_stop
    p2_setup --score $LONG --captains 2 --followers 8 --tempo 300 || return
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    wait_until 10 beat_at_least 5
    local out code
    out=$(ctl stop); code=$?
    [ $code = 0 ]; check $? "swarmctl stop answers OK ($out)"
    finish center 5; code=$CODE
    [ "$code" = 0 ]; check $? "the center stops at once and exits 0 (status $code)"
    expect_result 'beats=>4' 'beats=<144' collisions=0
    audit
    all_exit 5
    nothing_left
}

p2_sigint() {
    begin p2_sigint
    p2_setup --score $LONG --captains 2 --followers 8 --tempo 300 || return
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    wait_until 10 beat_at_least 5
    kill -INT "${PID[center]}" 2>/dev/null
    local code
    finish center 5; code=$CODE
    [ "$code" = 0 ]; check $? "on SIGINT the center ends the show cleanly and exits 0 (status $code)"
    grep -q '^RESULT ' "$LOGS/$SC/center.out"; check $? "it still prints its RESULT line"
    audit
    all_exit 5
    nothing_left
}

p2_crash() {
    begin p2_crash
    p2_setup --score $SCORE --captains 2 --followers 8 --tempo 240 || return
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    wait_until 10 beat_at_least 4
    kill -9 "${PID[captain_a]}" 2>/dev/null
    finish captain_a 2
    local code
    finish center 40; code=$CODE
    [ "$code" = 0 ]; check $? "a captain killed with kill -9: the show goes on to its end (status $code)"
    expect_result beats=56 dead=1 collisions=0 speed_violations=0
    audit
    all_exit 5
    nothing_left
}

p2_standby() {
    begin p2_standby
    p2_setup --score $SCORE --captains 2 --followers 8 --tempo 240 || return
    start captain_a -- bin/captain --center "$CENTER"
    start captain_b -- bin/captain --center "$CENTER"
    wait_until 10 status_has captains 2
    start standby -- bin/captain --center "$CENTER"
    wait_until 10 status_has standby 1
    check $? "a third captain waits as a standby ($(ctl status))"
    wait_until 10 beat_at_least 4
    local out code
    out=$(ctl kill 0); code=$?
    [ $code = 0 ]; check $? "swarmctl kill 0 answers OK ($out)"
    finish center 40; code=$CODE
    [ "$code" = 0 ]; check $? "the show goes on to its end (status $code)"
    expect_result beats=56 dead=1 takeovers=1 collisions=0 speed_violations=0
    audit
    all_exit 5
    nothing_left
}

# =============================================================================== Phase 3

p3_setup() {                         # p3_setup EXTRA-CENTER-ARGS...
    local tries
    CTL_BIND=127.0.0.51
    for tries in 1 2 3 4 5; do
        PORT=$((20000 + RANDOM % 30000))
        CENTER=tcp:127.0.0.2:$PORT
        start center SWARM_BIND=127.0.0.2 -- bin/center --listen "tcp::$PORT" --record "file:$REC" --wait 20 "$@"
        if wait_until 5 ctl status; then
            check 0 "the center listens on $CENTER (SWARM_BIND=127.0.0.2, listening on tcp::$PORT)"
            return 0
        fi
        finish center 1
    done
    check 1 "the center listens on tcp::PORT with SWARM_BIND=127.0.0.2 (see $LOGS/$SC/center.err)"
    return 1
}

captains() {                         # captains N [VAR=value ...]: radio captains on 127.0.0.11..
    local n=$1 i; shift
    for i in $(seq 1 "$n"); do
        start "captain_$i" SWARM_BIND=127.0.0.$((10 + i)) "$@" -- bin/captain --center "$CENTER" --radio udp::0
    done
}

performers() {                       # performers FIRST LAST [VAR=value ...]: on 127.0.1.FIRST..
    local a=$1 b=$2 i; shift 2
    for i in $(seq "$a" "$b"); do
        start "performer_$i" SWARM_BIND=127.0.1.$i "$@" -- bin/performer --center "$CENTER"
    done
}

p3_basic() {
    begin p3_basic
    p3_setup --score $SCORE --captains 2 --followers 4 --tempo 300 || return
    start viewer SWARM_BIND=127.0.0.50 -- bin/viewer --from "$CENTER" --check
    captains 2
    performers 1 8
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "a show of 2 captains and 8 performers, each on its own address, ends (status $code)"
    expect_result beats=56 drones=8 joined=8 collisions=0 speed_violations=0 dead=0 lost=0
    audit
    finish viewer 10; code=$CODE
    [ "$code" = 0 ] && [ "$(value finished "$LOGS/$SC/viewer.out")" = 1 ]
    check $? "a viewer on another machine sees the show to its end: $(grep '^CHECK' "$LOGS/$SC/viewer.out")"
    all_exit 6
}

p3_lossy() {
    begin p3_lossy
    p3_setup --score $SCORE --captains 2 --followers 4 --tempo 240 || return
    captains 2 SWARM_DROP=20
    performers 1 8 SWARM_DROP=20
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "the radio drops 20% of all datagrams: the show still ends (status $code)"
    expect_result beats=56 joined=8 collisions=0 speed_violations=0
    audit
    all_exit 6
}

p3_spare() {
    begin p3_spare
    p3_setup --score $SCORE --captains 2 --followers 4 --tempo 240 || return
    captains 2
    performers 1 8
    wait_until 10 beat_at_least 1
    performers 9 9                    # all slots are taken: a spare
    wait_until 10 beat_at_least 4
    kill -9 "${PID[performer_1]}" 2>/dev/null
    finish performer_1 2
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "a performer killed with kill -9: the show goes on (status $code)"
    expect_result beats=56 lost=1 joined=9 collisions=0 speed_violations=0
    audit
    all_exit 6
}

p3_rescue() {
    begin p3_rescue
    p3_setup --score $SCORE --captains 2 --followers 4 --tempo 240 || return
    captains 2
    performers 1 8
    wait_until 10 status_has captains 2
    start standby SWARM_BIND=127.0.0.13 -- bin/captain --center "$CENTER" --radio udp::0
    wait_until 10 status_has standby 1
    check $? "a third captain waits as a standby ($(ctl status))"
    wait_until 10 beat_at_least 4
    ctl kill 0 >/dev/null
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "captain 0 crashes: the show goes on (status $code)"
    expect_result beats=56 dead=1 takeovers=1 'rehomed=>3' landed=0 collisions=0 speed_violations=0
    audit
    all_exit 6
}

p3_land() {
    begin p3_land
    p3_setup --score $SCORE --captains 2 --followers 4 --tempo 240 || return
    captains 2
    performers 1 8
    wait_until 10 beat_at_least 4
    kill -9 "${PID[captain_1]}" 2>/dev/null
    finish captain_1 2
    local code
    finish center 60; code=$CODE
    [ "$code" = 0 ]; check $? "a captain killed with no standby: the show goes on (status $code)"
    expect_result beats=56 dead=1 landed=4 collisions=0 speed_violations=0
    audit
    all_exit 6
}

# ================================================================================== main

PHASE2="p2_basic p2_operator p2_stop p2_sigint p2_crash p2_standby"
PHASE3="p3_basic p3_lossy p3_spare p3_rescue p3_land"
case ${1:-} in
    phase2) shift; list=${*:-$PHASE2} ;;
    phase3) shift; list=${*:-$PHASE3} ;;
    list)   echo "phase2: $PHASE2"; echo "phase3: $PHASE3"; exit 0 ;;
    *)      echo "usage: tests/check.sh phase2|phase3 [scenario ...] | list" >&2; exit 2 ;;
esac
need_programs
for s in $list; do
    "$s"
done
kill_all
echo "$PASSES passed, $FAILURES failed (logs in tests/logs)"
exit $((FAILURES > 255 ? 255 : FAILURES))
