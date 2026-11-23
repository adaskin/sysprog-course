#!/bin/sh
# usage: sh tools/test_mode.sh MODE   (run from the skeleton directory)
# Starts ./examsrv MODE on a free port, runs the conformance checks and a load test,
# stops the server, and prints PASS or FAIL for the mode.
MODE=${1:?usage: test_mode.sh thread|fork|select|poll|epoll}
CLIENTS=${CLIENTS:-300}
rm -rf results && mkdir -p results
./examsrv "$MODE" 0 > .srv.out 2> .srv.err &
SRV=$!
for i in 1 2 3 4 5 6 7 8 9 10; do
    grep -q LISTENING .srv.out 2>/dev/null && break
    kill -0 $SRV 2>/dev/null || break
    sleep 0.2
done
sleep 0.3
if ! kill -0 $SRV 2>/dev/null; then
    echo "== $MODE: FAIL (server exited right after start: $(head -1 .srv.err))"
    rm -f .srv.out .srv.err
    exit 1
fi
PORT=$(awk '/LISTENING/ {print $2}' .srv.out)
if [ -z "$PORT" ]; then
    echo "== $MODE: FAIL (server did not start: $(head -1 .srv.err))"
    wait $SRV 2>/dev/null
    rm -f .srv.out .srv.err
    exit 1
fi
python3 tools/loadtest.py --port "$PORT" --pid $SRV --clients "$CLIENTS" --label "$MODE"
RC=$?
kill $SRV 2>/dev/null
wait $SRV 2>/dev/null
rm -f .srv.out .srv.err
exit $RC
