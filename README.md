# tiny-redis

A single-threaded, in-memory key-value store in C++20. It speaks RESP, the Redis wire
protocol, so `redis-cli` and `redis-benchmark` work against it unmodified. epoll event
loop, RESP parser, TTL expiry, and a hash table whose entries come from a custom pool
allocator.

> **Status:** in development. No benchmark has run yet, so the table below is empty.

## Benchmark

<!-- TODO(robbie): fill in from a real run.
     - Every number comes from a committed results file.
     - Median of N trials after warmup, never a single run.
     - p50 and p99, never an average.
     - State hardware, compiler, flags and core pinning below the table.
     - Add a redis-server column only if it ran on the same box with the same flags. -->

| Workload | tiny-redis | p50 | p99 |
|---|---|---|---|
| SET, no pipelining | — | — | — |
| GET, no pipelining | — | — | — |
| SET, pipelined (`-P 16`) | — | — | — |
| GET, pipelined (`-P 16`) | — | — | — |

Hardware / toolchain: _TBD_

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
`PING`

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
