#!/usr/bin/env bash
# Poor man's profiler. Samples the server's stack with gdb while redis-benchmark loads it,
# then counts identical stacks. Needs only gdb, so it works without perf or extra container
# capabilities. Runs inside the dev container.
#
#   scripts/pmp.sh <build-dir> [samples] [depth]      defaults: 20 samples, 8 frames
#
# Each sample stops the process for tens of ms, so this finds where time goes, not how
# much. Default load is SET with 50 clients at -P 16. PMP_LOAD="<redis-benchmark args>"
# replaces it.
set -u

BIN=${1:?usage: pmp.sh <build-dir> [samples] [depth]}/tiny-redis-server
SAMPLES=${2:-20}
DEPTH=${3:-8}
PORT=${PMP_PORT:-6391}
LOAD=${PMP_LOAD:--t set -c 50 -P 16}

[ -x "$BIN" ] || { echo "no server binary at $BIN" >&2; exit 1; }

"$BIN" --port "$PORT" >/dev/null 2>&1 &
SRV=$!
sleep 0.5
# shellcheck disable=SC2086
redis-benchmark -p "$PORT" -n 1000000000 -q $LOAD >/dev/null 2>&1 &
LD=$!
trap 'kill $LD $SRV 2>/dev/null' EXIT
sleep 1

for _ in $(seq "$SAMPLES"); do
  # Innermost frame first, joined with " <- ". Strips addresses, aarch64 [PAC] tags, source
  # locations and argument lists; a "(" after a name starts the arguments.
  gdb -p "$SRV" -batch -ex "bt $DEPTH" 2>/dev/null \
    | grep '^#' \
    | sed -E 's/^#[0-9]+ +//; s/0x[0-9a-f]+ (\[PAC\] )?in //; s/ (at|from) .*//; s/([A-Za-z0-9_>]) ?\(.*/\1/; s/^ *$/??/' \
    | paste -sd'\t' - | sed 's/\t/ <- /g'
done | sort | uniq -c | sort -rn
