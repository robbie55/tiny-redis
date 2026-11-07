#!/usr/bin/env bash
# Throughput benchmark: tiny-redis vs redis-server, same box, same client, same flags.
# Runs inside the dev container on the release build.
#
#   scripts/bench.sh <out-dir> [trials]          default 5 trials
#
# Matrix: SET and GET x pipeline 1 and 16, 50 clients, 256 B values, 100k random keys.
# Each trial restarts both servers, alternating which goes first so drift (thermal, host
# load) doesn't favour one. Each start is preloaded, so GETs hit. The server is pinned to
# one vCPU, the client to two others. Raw redis-benchmark CSV plus server and client CPU use
# for every run land in <out-dir>/raw; summary.csv holds the medians.
set -euo pipefail

OUT=${1:?usage: bench.sh <out-dir> [trials]}
TRIALS=${2:-5}
BIN=build/release/tiny-redis-server
PORT=6393
SERVER_CPU=1
CLIENT_CPUS=2-5
KEYS=100000
VALUE=256
CLIENTS=50

[ -x "$BIN" ] || { echo "no release build at $BIN" >&2; exit 1; }
# Docs cite result directories by path, so a run must never overwrite one.
[ -e "$OUT" ] && { echo "$OUT already exists; pick a new directory" >&2; exit 1; }
mkdir -p "$OUT/raw"

# Persistence off and one I/O thread for Redis: single-threaded vs single-threaded, nothing
# on disk.
server_cmd() {
  case "$1" in
    tiny-redis) echo "$BIN --port $PORT" ;;
    # latency-tracking is on by default in 7.0 and costs Redis time tiny-redis doesn't spend.
    redis) echo "redis-server --port $PORT --save \"\" --appendonly no --io-threads 1 --latency-tracking no" ;;
  esac
}

# Requests per run, sized for 10 s or more. redis-benchmark times runs in 0.25 s steps,
# so shorter runs quantize ops/s badly (4 s runs: ~6% per step).
requests() { [ "$1" = 1 ] && echo 3000000 || echo 12000000; }

client() { taskset -c "$CLIENT_CPUS" redis-benchmark -p $PORT --threads 4 -c $CLIENTS -r $KEYS -d $VALUE "$@"; }

# CPU seconds the process has used (user + system).
cpu_secs() { awk -v hz="$(getconf CLK_TCK)" '{ printf "%.2f", ($14 + $15) / hz }' "/proc/$1/stat"; }

# Sets CHILD to the CPU seconds used by this shell's finished children (the client, once
# waited on). Call it bare, never as $(child_secs): `times` in a subshell sees only the
# subshell's children.
TIMES=$(mktemp)
child_secs() {
  times >"$TIMES"
  CHILD=$(tail -1 "$TIMES" | tr 'ms' '  ' | awk '{ printf "%.2f", $1 * 60 + $2 + $3 * 60 + $4 }')
}

pct() { awk -v a="$1" -v a0="$2" -v w="$3" -v w0="$4" 'BEGIN { printf "%.0f\n", 100 * (a - a0) / (w - w0) }'; }

{
  echo "date:     $(date -u +%FT%TZ)"
  echo "commit:   $(git rev-parse --short HEAD)$(git diff --quiet HEAD -- src || echo ' (dirty src)')"
  echo "build:    $(grep -E '^CMAKE_(BUILD_TYPE|CXX_FLAGS_RELEASE|CXX_COMPILER):' build/release/CMakeCache.txt | tr '\n' ' ')"
  echo "kernel:   $(uname -srm)"
  echo "client:   $(redis-benchmark --version)"
  echo "baseline: $(redis-server --version)"
  echo "pinning:  server vcpu $SERVER_CPU, client vcpus $CLIENT_CPUS (--threads 4)"
  echo "workload: -c $CLIENTS -r $KEYS -d $VALUE, trials $TRIALS"
  for s in tiny-redis redis; do echo "start:    taskset -c $SERVER_CPU $(server_cmd $s)"; done
  echo "run:      taskset -c $CLIENT_CPUS redis-benchmark -p $PORT --threads 4 -c $CLIENTS -r $KEYS -d $VALUE -t <set|get> -P <p> -n <n> --csv"
} >"$OUT/env.txt"
cat "$OUT/env.txt"

for t in $(seq "$TRIALS"); do
  order="tiny-redis redis"; [ $((t % 2)) = 0 ] && order="redis tiny-redis"
  for server in $order; do
    # exec, so $! is the server itself: its /proc entry gives CPU use and kill reaches it.
    bash -c "exec taskset -c $SERVER_CPU $(server_cmd $server)" >>"$OUT/raw/$server.log" 2>&1 &
    pid=$!
    for _ in $(seq 50); do redis-cli -p $PORT ping >/dev/null 2>&1 && break; sleep 0.1; done
    kill -0 $pid 2>/dev/null || { echo "$server failed to start:" >&2; cat "$OUT/raw/$server.log" >&2; exit 1; }
    client -t set -n 1000000 -q >/dev/null 2>&1
    keys=$(redis-cli -p $PORT dbsize)
    echo "$keys" >"$OUT/raw/$server-t$t.dbsize"
    [ "$keys" -ge $((KEYS * 99 / 100)) ] || { echo "$server preload left only $keys keys" >&2; exit 1; }

    for cmd in set get; do
      for p in 1 16; do
        name="$server-$cmd-P$p-t$t"
        s0=$(cpu_secs $pid); child_secs; c0=$CHILD; w0=$(date +%s.%N)
        client -t "$cmd" -P "$p" -n "$(requests "$p")" --csv >"$OUT/raw/$name.csv" 2>/dev/null
        s1=$(cpu_secs $pid); child_secs; c1=$CHILD; w1=$(date +%s.%N)
        pct "$s1" "$s0" "$w1" "$w0" >"$OUT/raw/$name.cpu"
        pct "$c1" "$c0" "$w1" "$w0" >"$OUT/raw/$name.clientcpu"
        printf '%-28s %s  server cpu %s%%  client cpu %s%%\n' "$name" \
          "$(tail -1 "$OUT/raw/$name.csv" | cut -d, -f2)" "$(cat "$OUT/raw/$name.cpu")" "$(cat "$OUT/raw/$name.clientcpu")"
      done
    done

    kill $pid; wait $pid 2>/dev/null || true
  done
done

# Medians across trials. Columns from redis-benchmark --csv: test, rps, avg, min, p50, p95, p99, max.
python3 - "$OUT" "$TRIALS" <<'PY'
import csv, glob, statistics, sys
out, trials = sys.argv[1], int(sys.argv[2])
rows = []
for server in ("tiny-redis", "redis"):
    for cmd in ("set", "get"):
        for p in (1, 16):
            rps, p50, p99, cpu, ccpu = [], [], [], [], []
            for t in range(1, trials + 1):
                base = f"{out}/raw/{server}-{cmd}-P{p}-t{t}"
                rec = list(csv.reader(open(base + ".csv")))[-1]
                rps.append(float(rec[1])); p50.append(float(rec[4])); p99.append(float(rec[6]))
                cpu.append(int(open(base + ".cpu").read()))
                ccpu.append(int(open(base + ".clientcpu").read()))
            rows.append([server, cmd.upper(), p, round(statistics.median(rps)), round(min(rps)),
                         round(max(rps)), round(statistics.median(p50), 3), round(statistics.median(p99), 3),
                         statistics.median(cpu), statistics.median(ccpu)])
with open(f"{out}/summary.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["server", "command", "pipeline", "rps_median", "rps_min", "rps_max",
                "p50_ms_median", "p99_ms_median", "server_cpu_pct_median",
                "client_cpu_pct_median"])
    w.writerows(rows)
PY
echo; tr , "\t" <"$OUT/summary.csv"
