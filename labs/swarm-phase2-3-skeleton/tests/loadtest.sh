#!/bin/bash
# loadtest.sh — the Phase 3 load test (given; `make loadtest`). Run it from the project directory.
#
# One center (TCP), LEADERS captains with FOLLOWERS performer processes each, a headless TCP
# viewer, the long score, as fast as possible (tempo 0). Prints one table row.
#
#   tests/loadtest.sh                              4 captains x 16 performers, long.score
#   LEADERS=8 FOLLOWERS=32 tests/loadtest.sh       bigger
#   tests/loadtest.sh --workers 1 --queue-cap 2    any center option: change ONE parameter at a time
#   SWARM_UDP_LOSS=10 tests/loadtest.sh            a noisy radio
#   HEADER=0 tests/loadtest.sh ...                 the row only (to build a table in a loop)
#
# Columns: drones, beats, seconds, beats/s, BUSY replies, late beats, messages from the captains,
# messages/s, frames the viewer drew, collisions + speed violations (must be 0), mosaic accuracy.
cd "$(dirname "$0")/.." || exit 1
LEADERS=${LEADERS:-4}
FOLLOWERS=${FOLLOWERS:-16}
SCORE=${SCORE:-data/scores/long.score}
LOGS=tests/logs
mkdir -p "$LOGS"
n="load$$"

bin/center --name "$n" --port 0 --paused --tempo 0 --timeout 120 --score "$SCORE" \
    --leaders "$LEADERS" --followers "$FOLLOWERS" "$@" >"$LOGS/load.out" 2>"$LOGS/load.log" &
cp=$!
trap 'kill -KILL $cp 2>/dev/null' EXIT
for _ in $(seq 50); do
    P=$(grep -o 'listening on tcp port [0-9]*' "$LOGS/load.log" | awk '{print $5}')
    [ -n "$P" ] && break
    sleep 0.1
done
if [ -z "$P" ]; then
    echo "loadtest: the center did not start (see $LOGS/load.log)" >&2
    exit 1
fi
for _ in $(seq 100); do                              # wait until every captain is connected
    [ "$(grep -c 'connected from' "$LOGS/load.log")" -ge "$LEADERS" ] && break
    sleep 0.1
done
bin/viewer --center "127.0.0.1:$P" --view check >"$LOGS/load.view" 2>&1 &
vp=$!
sleep 1                                              # the performers start and join
bin/swarmctl --center "127.0.0.1:$P" resume >/dev/null || { echo "loadtest: RESUME failed" >&2; exit 1; }
wait $cp
rc=$?
wait $vp 2>/dev/null
trap - EXIT

R=$(grep '^RESULT' "$LOGS/load.out")
C=$(grep '^CHECK' "$LOGS/load.view")
f() { echo "$1" | grep -o "\b$2=[-0-9.]*" | head -1 | cut -d= -f2; }
if [ -z "$R" ]; then
    echo "loadtest: no RESULT (exit $rc); see $LOGS/load.log" >&2
    exit 1
fi
el=$(f "$R" elapsed); beats=$(f "$R" beats); msgs=$(f "$R" msgs)
viol=$(( $(f "$R" collisions) + $(f "$R" speed_violations) ))
if [ "${HEADER:-1}" = 1 ]; then
    printf "%-7s %-6s %-7s %-8s %-6s %-5s %-8s %-9s %-7s %-6s %-6s %s\n" \
        drones beats seconds beats/s busy late msgs msgs/s frames unsafe acc options
fi
printf "%-7s %-6s %-7s %-8s %-6s %-5s %-8s %-9s %-7s %-6s %-6s %s\n" \
    "$(f "$R" drones)" "$beats" "$el" "$(awk -v b="$beats" -v e="$el" 'BEGIN{printf("%.1f", (e > 0) ? b / e : 0)}')" \
    "$(f "$R" busy)" "$(f "$R" late)" "$msgs" "$(awk -v m="$msgs" -v e="$el" 'BEGIN{printf("%.0f", (e > 0) ? m / e : 0)}')" \
    "$(f "$C" frames)" "$viol" "$(f "$R" mosaic_accuracy)" "${*:-default}${SWARM_UDP_LOSS:+ loss=$SWARM_UDP_LOSS%}"
[ $rc -eq 0 ] && [ "$viol" = 0 ]
