#!/bin/sh
# GIVEN: protocol checks, N real student clients, fd-leak and zombie checks.
MODE=${1:?usage: test_server.sh iterative|thread|fork|select|poll|epoll [N]}
N=${2:-}
[ -z "$N" ] && { [ "$MODE" = iterative ] && N=20 || N=300; }
SKIP=""
[ "$MODE" = iterative ] && SKIP="--skip-idle"
rm -rf results && mkdir -p results
fail() {
    echo "== $MODE: FAIL ($1)"
    kill "$SRV" 2>/dev/null
    wait "$SRV" 2>/dev/null
    rm -f .srv.out .srv.err
    exit 1
}
bin/examsrv "$MODE" 0 > .srv.out 2> .srv.err &
SRV=$!
for i in 1 2 3 4 5 6 7 8 9 10; do
    grep -q LISTENING .srv.out 2>/dev/null && break
    sleep 0.2
done
sleep 0.3
kill -0 "$SRV" 2>/dev/null || fail "server exited after start: $(head -1 .srv.err)"
PORT=$(awk '/LISTENING/ {print $2}' .srv.out)
FD0=$(ls "/proc/$SRV/fd" | wc -l)
echo "== $MODE: protocol checks"
python3 tools/check_server.py --port "$PORT" $SKIP || fail "protocol checks"
echo "== $MODE: $N students at once"
sh tools/run_students.sh "$N" "$PORT" 100 || fail "some students failed"
sleep 0.5
FD1=$(ls "/proc/$SRV/fd" 2>/dev/null | wc -l)
ZOMB=$(ps -o stat= --ppid "$SRV" 2>/dev/null | grep -c '^Z')
echo "   server fds: $FD0 -> $FD1   zombie children: $ZOMB"
[ "$FD1" -le "$FD0" ] || fail "file descriptor leak"
[ "$ZOMB" -eq 0 ] || fail "zombie children"
kill "$SRV" 2>/dev/null
wait "$SRV" 2>/dev/null
rm -f .srv.out .srv.err
echo "== $MODE: PASS"
