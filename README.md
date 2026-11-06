# tiny-redis

A single-threaded, in-memory key-value store in C++20. It speaks RESP, the Redis wire
protocol, so `redis-cli` and `redis-benchmark` work against it unmodified. epoll event
loop, RESP parser, TTL expiry, and a hash table whose entries come from a custom pool
allocator.

> **Status:** in development. The numbers below come from one run on a laptop VM. Read the
> caveats in [`docs/benchmark.md`](docs/benchmark.md) before quoting them.

## Benchmark

Same box, same client, same flags, and each server pinned to the same vCPU. The baseline
is `redis-server` 7.0.15 with persistence off and one I/O thread. Values are 256 bytes,
with 50 clients and 100k keys. Each number is the median of 5 alternating trials, and the
server was at 98–100% CPU in every run.

| Workload | tiny-redis ops/s | redis ops/s | ratio |
|---|---|---|---|
| SET, no pipelining | 210,467 | 196,618 | 1.07× (within noise) |
| GET, no pipelining | 206,840 | 199,920 | 1.03× (within noise) |
| SET, pipelined (`-P 16`) | 1,090,314 | 799,254 | 1.36× |
| GET, pipelined (`-P 16`) | 1,169,819 | 904,500 | 1.29× |

Hardware: Apple M1 Pro, Docker Desktop Linux VM (aarch64), gcc 13.3 `-O3`. Source:
[`bench/results/2026-11-05-docker/`](bench/results/2026-11-05-docker/). Latencies, method
and caveats are in [`docs/benchmark.md`](docs/benchmark.md). tiny-redis does much less
per command than Redis. Treat the ratio as a measure of this server's speed, not as a
claim that it replaces Redis.

## Architecture

<!-- TODO(robbie): the path a request takes, socket -> event loop -> per-connection
     buffers -> RESP parse -> dispatch -> store, and where the pool allocator sits. -->

_TBD._

### Design notes

<!-- TODO(robbie): write this last, from decisions actually made. For each one, name the
     alternative and why it lost. Candidates:
       - how parsing avoids copies, and what that requires of buffer lifetime
       - how many write() calls a pipelined batch costs, and why
       - one clock read per event-loop iteration
       - the fixed-capacity pool, and what happens at the cap
       - entry layout and sizeof(Entry)
       - hash seeding -->

_TBD._

## Commands

`GET` · `SET key value [EX s | PX ms | KEEPTTL]` · `DEL` · `EXISTS` · `EXPIRE` · `TTL` ·
`PING` · `ECHO` · `DBSIZE` · `FLUSHALL` · `QUIT`

Both wire forms are accepted: RESP arrays of bulk strings (what every real client sends)
and inline commands (`PING\r\n` typed at a socket). Keys and values are binary-safe.

Three limits are enforced when the header is parsed, before any payload is buffered: 64 MiB
per bulk string, 64 KiB per inline command line, and 16 arguments per command. Exceeding one
is a protocol error. The bulk length is a number the client chooses, so it is checked before
it is trusted — `$4294967295` costs the sender eleven bytes. The inline cap is far lower
because an inline command carries no length at all: the only way to find its end is to scan
for a terminator, so an unterminated line has to be buffered until the cap says stop. For
reference, the equivalent knob in Redis is `proto-max-bulk-len`, which defaults to 512MB.

## Build

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release

./build/release/tiny-redis-server --port 6380
redis-cli -p 6380 SET hello world
```

Presets: `release`, `debug`, `asan` (AddressSanitizer + UBSan), `tsan`.

epoll is Linux-only, so `tiny-redis-server` only builds on Linux. The store and protocol
layers also build and pass their tests on macOS, and CI runs that job, so neither can pick
up a dependency on the event loop.

A Docker-based Linux environment is included for development on other platforms:

```sh
./scripts/dev.sh up
./scripts/dev.sh test debug
```

See [`docs/development.md`](docs/development.md) for the full command list.

## License

MIT
