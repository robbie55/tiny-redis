# tiny-redis

A single-threaded, in-memory key-value store in C++20 that speaks the real Redis wire
protocol, so `redis-cli` and `redis-benchmark` talk to it unmodified. epoll event loop,
RESP parser, TTL expiry, and a hash table whose entries are carved from a custom pool
allocator.

> **Status: in development.** No benchmark has been run yet. The table below stays empty
> until there is a real measurement behind it.

## Benchmark

<!-- TODO(robbie): fill in from a real run. Rules for this table:
     - Every number comes from a committed results file. No number goes in from memory.
     - Median of N trials after warmup, never a single run.
     - p50 AND p99. An average latency is not defensible in an interview.
     - State the hardware, compiler, flags, and core pinning below the table.
     - If you measure real redis-server as a reference point, say so and use the same
       flags on the same box. If you did not, drop that column entirely. -->

| Workload | tiny-redis | p50 | p99 |
|---|---|---|---|
| SET, no pipelining | — | — | — |
| GET, no pipelining | — | — | — |
| SET, pipelined (`-P 16`) | — | — | — |
| GET, pipelined (`-P 16`) | — | — | — |

Hardware / toolchain: _TBD_

## Architecture

<!-- TODO(robbie): draw this once the design is actually yours. It should show the path a
     request takes: socket -> event loop -> per-connection buffers -> RESP parse ->
     dispatch -> store, and where the pool allocator sits. -->

_TBD._

### Design notes

<!-- TODO(robbie): this section is what a reader skims to decide whether you know your own
     project. Write it last, from decisions you actually made, and give the reasoning --
     not just what it does but what the alternative was and why you rejected it.
     Candidates worth writing up:
       - how parsing avoids copying, and what constrains buffer lifetime
       - how many write() calls a pipelined batch costs, and why
       - clock sampling per event-loop iteration
       - the fixed-capacity pool, and what happens at the cap
       - entry layout and why sizeof(Entry) is what it is
       - hash seeding -->

_TBD._

## Commands

`GET` · `SET key value [EX s | PX ms | KEEPTTL]` · `DEL` · `EXISTS` · `EXPIRE` · `TTL` ·
`PING`

Both wire forms are accepted: RESP arrays of bulk strings (what every real client sends)
and inline commands (`PING\r\n` typed at a socket). Keys and values are binary-safe.

## Build

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release

./build/release/tiny-redis-server --port 6380
redis-cli -p 6380 SET hello world
```

Presets: `release`, `debug`, `asan` (AddressSanitizer + UBSan), `tsan`.

epoll is Linux-only, so `tiny-redis-server` builds on Linux. The store and protocol layers
are portable and are built and unit-tested on macOS too, which keeps them from growing a
dependency on the event loop.

## License

MIT
