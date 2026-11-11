#!/bin/bash
# Run the whole LuthierTests suite as N exact, disjoint shards in parallel processes.
#
# Usage: scripts/agents/shards.sh <test-binary> <shards> <output-dir> [suite-filter-regex]
# Exit:  0 when every shard exits 0, else 1. Per-shard logs: <output-dir>/shard-K.log
#
# Each shard runs the binary with --skip=<suite> for every suite that belongs to another shard
# (suites are dealt round-robin in registration order), so the union is exactly the full suite
# list and no test runs twice. A shard with no summary line crashed or was killed.
# Each shard has its own HOME, and runs from the build directory (see test.sh for why).
set -uo pipefail
BIN="$1"; N="$2"; OUT="$3"; FILTER="${4:-.}"
mkdir -p "$OUT"
[ -x "$BIN" ] || { echo "shards.sh: not executable: $BIN" >&2; exit 2; }
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
OUT="$(cd "$OUT" && pwd)"
BUILD_DIR="$(cd "$(dirname "$BIN")/../.." && pwd)"

( cd "$BUILD_DIR" && xvfb-run -a "$BIN" --list 2>/dev/null ) | sed -n 's/ :: .*$//p' | awk '!seen[$0]++' > "$OUT/suites.all"
grep -E "$FILTER" "$OUT/suites.all" > "$OUT/suites.sel" || true
total=$(wc -l < "$OUT/suites.sel")
[ "$total" -gt 0 ] || { echo "shards.sh: no suites selected" >&2; exit 2; }
echo "shards.sh: $total suites in $N shards" >&2

pids=()
for k in $(seq 0 $((N - 1))); do
    awk -v n="$N" -v k="$k" '(NR - 1) % n == k' "$OUT/suites.sel" > "$OUT/shard-$k.suites"
    args=()
    while IFS= read -r s; do
        grep -qxF -- "$s" "$OUT/shard-$k.suites" || args+=("--skip=$s")
    done < "$OUT/suites.all"
    mkdir -p "$OUT/home-$k"
    ( cd "$BUILD_DIR" && HOME="$OUT/home-$k" XDG_CONFIG_HOME="$OUT/home-$k/.config" \
      XDG_DATA_HOME="$OUT/home-$k/.local/share" XDG_CACHE_HOME="$OUT/home-$k/.cache" \
      xvfb-run -a "$BIN" "${args[@]}" > "$OUT/shard-$k.log" 2> "$OUT/shard-$k.err"
      echo "$?" > "$OUT/shard-$k.rc" ) &
    pids+=("$!")
done
for p in "${pids[@]}"; do wait "$p"; done

rc=0
for k in $(seq 0 $((N - 1))); do
    r=$(cat "$OUT/shard-$k.rc" 2>/dev/null || echo 99)
    summary=$(grep -E '^ (ALL PASSED|FAILED)$' -A1 "$OUT/shard-$k.log" | tail -1)
    echo "shard $k: rc=$r ${summary:-<no summary line: crash or timeout>}"
    [ "$r" = 0 ] || rc=1
done
exit $rc
