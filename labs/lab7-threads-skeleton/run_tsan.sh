#!/bin/bash
# Runs the ThreadSanitizer builds of the fixed programs (given). Use `make tsan`.
cd "$(dirname "$0")" || exit 1
fail=0
export TSAN_OPTIONS="halt_on_error=1 exitcode=66"

./tokencount_tsan tests/short.txt 4 >/dev/null 2>&1 && echo "PASS  tokencount_tsan (short)" || { echo "FAIL  tokencount_tsan (short)"; fail=1; }
[ -f data/big.txt ] && { ./tokencount_tsan data/big.txt 8 >/dev/null 2>&1 && echo "PASS  tokencount_tsan (big)" || { echo "FAIL  tokencount_tsan (big)"; fail=1; }; }
./race_fixed_tsan 4 100000 >/dev/null 2>&1 && echo "PASS  race_fixed_tsan" || { echo "FAIL  race_fixed_tsan"; fail=1; }
exit $fail
