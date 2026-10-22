#!/bin/bash
# Lab 7 checks (given). Run with `make test`. Prints PASS/FAIL per check; exit status 1 if any fail.
cd "$(dirname "$0")/.." || exit 1

fail=0
pass() { echo "PASS  $1"; }
bad()  { echo "FAIL  $1"; fail=1; }

# ---- Task 1: totals identical over 10 runs, in all three modes
for mode in "" "--exit" "--detach"; do
    first=""
    ok=1
    for i in $(seq 10); do
        out=$(./agents $mode 2>/dev/null | grep '^total = ')
        [ -z "$first" ] && first="$out"
        [ -z "$out" ] || [ "$out" != "$first" ] && ok=0
    done
    [ $ok = 1 ] && pass "agents $mode: same total over 10 runs ($first)" || bad "agents $mode: total missing or varies"
done

# ---- Task 2: tokencount matches wc -w
[ -f tests/long_line.txt ] || { for i in $(seq 20000); do printf 'word%d ' "$i"; done > tests/long_line.txt; echo >> tests/long_line.txt; }
files="tests/empty.txt tests/one_word.txt tests/short.txt tests/mixed_ws.txt tests/no_newline.txt tests/long_line.txt"
[ -f data/big.txt ] && files="$files data/big.txt"
for f in $files; do
    expect=$(wc -w < "$f" | tr -d ' ')
    ok=1
    for n in 1 2 3 7 16 64; do
        got=$(./tokencount "$f" "$n" 2>/dev/null)
        [ "$got" = "$expect" ] || { ok=0; echo "      $f with $n threads: got '$got', wc -w says $expect"; }
    done
    [ $ok = 1 ] && pass "tokencount $f = $expect (1,2,3,7,16,64 threads)" || bad "tokencount $f"
done
[ -f data/big.txt ] || echo "SKIP  data/big.txt missing (run: make data)"

# ---- Task 3: race_fixed exact over 20 runs for several thread counts
for n in 1 2 4 8; do
    ok=1
    for i in $(seq 20); do
        out=$(./race_fixed "$n" 200000 2>/dev/null)
        s=$(echo "$out" | sed -n 's/^sum    = \([0-9]*\) (expected \([0-9]*\))$/\1 \2/p')
        d=$(echo "$out" | sed -n 's/^n_done = \([0-9]*\) (expected \([0-9]*\))$/\1 \2/p')
        [ -n "$s" ] && [ -n "$d" ] && [ "${s% *}" = "${s#* }" ] && [ "${d% *}" = "${d#* }" ] || ok=0
    done
    [ $ok = 1 ] && pass "race_fixed exact with $n threads (20 runs)" || bad "race_fixed with $n threads"
done

# ---- Task 4: strtok_r never fails
./tok_race --safe >/dev/null 2>&1 && pass "tok_race --safe never fails" || bad "tok_race --safe"

# ---- No locks or atomics in the fixed programs (they are next week)
if grep -nE 'pthread_mutex|sem_|atomic|volatile' agents.c tokencount.c race_fixed.c; then
    bad "locks/atomics/volatile found in the fixed programs"
else
    pass "no locks, atomics, or volatile in agents.c, tokencount.c, race_fixed.c"
fi

exit $fail
