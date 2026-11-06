#!/usr/bin/env bash
# End-to-end smoke test. Starts a server, drives it with redis-cli and redis-benchmark,
# and checks it survives abusive clients. Runs inside the dev container.
#
#   tests/e2e/smoke.sh <build-dir>      e.g. build/release, build/asan
#
# Exit status is the number of failed checks. Expected replies are hand-written from
# Redis's docs, not diffed against a real redis-server.
set -u

BIN=${1:?usage: smoke.sh <build-dir>}/tiny-redis-server
PORT=${SMOKE_PORT:-6390}
LOG=$(mktemp)
fails=0

[ -x "$BIN" ] || { echo "no server binary at $BIN" >&2; exit 1; }

cli() { redis-cli -p "$PORT" "$@"; }
check() { # name expected actual
  if [ "$2" == "$3" ]; then echo "PASS $1"; else echo "FAIL $1: want [$2] got [$3]"; fails=$((fails + 1)); fi
}
alive() { check "alive after $1" PONG "$(cli ping 2>&1)"; }
# Throughput lines only; redis-benchmark redraws progress with bare \r.
bench() { redis-benchmark -p "$PORT" -q "$@" 2>&1 | tr '\r' '\n' | grep 'requests per second'; }

"$BIN" --port "$PORT" >"$LOG" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT
for _ in $(seq 50); do cli ping >/dev/null 2>&1 && break; sleep 0.1; done

echo "== commands"
check ping PONG "$(cli ping)"
check echo hi "$(cli echo hi)"
check set OK "$(cli set k v)"
check get v "$(cli get k)"
check exists 1 "$(cli exists k)"
check ttl-persistent -1 "$(cli ttl k)"
check expire 1 "$(cli expire k 100)"
check ttl 100 "$(cli ttl k)"
check set-px OK "$(cli set e v px 200)"
sleep 0.4
check expired-get "" "$(cli get e)"
check ttl-missing -2 "$(cli ttl e)"
check dbsize 1 "$(cli dbsize)"
check del 1 "$(cli del k)"
check del-again 0 "$(cli del k)"
check flushall OK "$(cli flushall)"
check dbsize-empty 0 "$(cli dbsize)"
cli set bin "$(printf 'a\tb')" >/dev/null
check binary-safe "$(printf 'a\tb')" "$(cli get bin)"
check unknown-cmd "ERR unknown command 'nope', with args beginning with: " "$(cli nope 2>&1)"
check wrong-arity "ERR wrong number of arguments for 'get' command" "$(cli get 2>&1)"
check inline +PONG "$(exec 3<>/dev/tcp/127.0.0.1/"$PORT"; printf 'PING\r\n' >&3; timeout 1 head -c 7 <&3 | tr -d '\r\n')"
check quit OK "$(cli quit)"
alive commands

echo "== load"
for p in 1 16; do
  out=$(bench -t set,get -n 100000 -P "$p")
  echo "$out" | sed 's/^/    /'
  check "redis-benchmark -P $p" 2 "$(echo "$out" | grep -c .)"
done
check "50 clients" 2 "$(bench -t set,get -n 200000 -c 50 | grep -c .)"
alive load

echo "== abusive clients"
# Disconnect with a full pipeline in flight and every reply unread.
for _ in $(seq 20); do
  ( exec 3<>/dev/tcp/127.0.0.1/"$PORT"
    for j in $(seq 2000); do printf 'SET k%d %d\r\n' "$j" "$j"; done >&3 ) 2>/dev/null
done
timeout 2 redis-benchmark -p "$PORT" -t set -n 10000000 -c 50 -P 16 -q >/dev/null 2>&1
alive "abrupt disconnects"
( exec 3<>/dev/tcp/127.0.0.1/"$PORT"; printf '*3\r\n$3\r\nSET\r\n$1\r\nk' >&3 ) 2>/dev/null
alive "half-written command"

kill -TERM $PID; wait $PID; rc=$?
trap - EXIT
check "clean exit on SIGTERM" 0 "$rc"
if grep -qE 'ERROR: (Address|Leak)Sanitizer|runtime error' "$LOG"; then
  echo "FAIL sanitizer report:"; sed 's/^/    /' "$LOG"; fails=$((fails + 1))
fi

echo "== $fails failure(s)"
exit "$fails"
