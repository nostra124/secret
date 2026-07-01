#!/bin/sh
# coverage.sh — build an instrumented secret, run every test layer that
# can run on this host, and report gcov line coverage per file + total.
#
# Honours $SECRET_COV_MIN (fail if total% is below it) and runs the
# SIT-gated bats paths (SECRET_SIT=1) plus the fault-injection suite.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

echo "coverage: building instrumented binary (-O0 --coverage -DSECRET_FAULTS)"
rm -f lib/*.gcno lib/*.gcda src/*.gcno src/*.gcda lib/*.o src/*.o bin/secret
for f in lib/*.c src/secret.c; do
	cc -O0 -g --coverage -DSECRET_FAULTS -std=c99 -Ilib -D_GNU_SOURCE -fPIC \
	   -c "$f" -o "${f%.c}.o"
done
mkdir -p bin
cc --coverage -o bin/secret lib/*.o src/secret.o

export SECRET_SIT=1
echo "coverage: running unit suite"
bats tests/unit/*.bats >/dev/null 2>&1 || echo "coverage: (some unit tests failed)"
if [ -d tests/sit ]; then
	echo "coverage: running SIT suite"
	bats tests/sit/*.bats >/dev/null 2>&1 || echo "coverage: (some SIT tests failed)"
fi
if [ -d tests/fault ]; then
	echo "coverage: running fault-injection suite"
	bats tests/fault/*.bats >/dev/null 2>&1 || echo "coverage: (some fault tests failed)"
fi

echo
printf '%-24s %8s %8s\n' "FILE" "COV%" "LINES"
echo "------------------------------------------------"
tl=0; tc=0
for f in lib/*.c src/secret.c; do
	out=$(gcov -n -o "$(dirname "$f")" "$f" 2>/dev/null | grep -A1 "File '$f'" | grep "Lines executed" || true)
	[ -z "$out" ] && continue
	pct=$(echo "$out" | sed -E 's/Lines executed:([0-9.]+)% of ([0-9]+)/\1/')
	ln=$(echo  "$out" | sed -E 's/Lines executed:([0-9.]+)% of ([0-9]+)/\2/')
	printf '%-24s %7s%% %8s\n' "$f" "$pct" "$ln"
	tl=$((tl + ln))
	tc=$((tc + $(awk "BEGIN{printf \"%d\", $pct*$ln/100 + 0.5}")))
done
echo "------------------------------------------------"
total=$(awk "BEGIN{printf \"%.1f\", $tc*100/$tl}")
printf '%-24s %7s%% %8s\n' "TOTAL" "$total" "$tl"

# Clean instrumentation artifacts (keep the report).
rm -f lib/*.gcno lib/*.gcda src/*.gcno src/*.gcda lib/*.o src/*.o bin/secret

min=${SECRET_COV_MIN:-0}
awk "BEGIN{exit ($total < $min) ? 1 : 0}" || {
	echo "coverage: $total% is below the $min% threshold" >&2
	exit 1
}
