# Development

Two build environments, on purpose.

| | where | what it builds | use it for |
|---|---|---|---|
| **native macOS** | your laptop | `tinyredis_core` + tests | fast iteration on `src/core` |
| **Docker (Linux)** | `scripts/dev.sh` | everything, incl. `tiny-redis-server` | sockets, ASan/TSan, benchmarks |

`epoll` is Linux-only, so `src/net` and every benchmark number come from the container.
`src/core` has no networking dependency and builds natively on macOS. That's faster to
iterate on, and if the store or protocol layer ever pulls in event-loop code, the macOS
build breaks.

## Native (macOS)

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, `asan`, `tsan`. On macOS these configure the portable core
and its tests only, and CMake prints a status line saying so.

## Container (Linux)

Requires Docker Desktop to be running. Everything goes through one script:

```sh
./scripts/dev.sh help
```

### First run

```sh
./scripts/dev.sh up          # builds the image (~2-3 min), starts the container
./scripts/dev.sh test debug  # checks toolchain, FetchContent and CTest end to end
./scripts/dev.sh hw          # writes docs/hardware.txt; commit it
```

`up` is idempotent, and every other command calls it for you. The container is
long-lived and `dev.sh` `exec`s into it, so commands after the first skip container
startup.

### Commands

| command | does |
|---|---|
| `up` | build image + start container. Idempotent. |
| `build [preset]` | configure + build. Default preset `debug`. |
| `test [preset] [-R regex]` | build, then `ctest`. `-R` filters, e.g. `test debug -R Buffer`. |
| `server [args...]` | build `release`, run `tiny-redis-server`. Extra args pass through. |
| `cli [args...]` | `redis-cli` against the running server, inside the container. |
| `bench [args...]` | `redis-benchmark` against it, inside the container. |
| `benchmark [trials]` | build `release`, then benchmark it against `redis-server`. Writes `bench/results/<date>-docker/`. Method in [`benchmark.md`](benchmark.md). |
| `smoke [preset]` | build, then run the end-to-end smoke test. Default preset `release`. |
| `pmp [preset] [samples] [depth]` | build, then sample the server's stacks with gdb under load. Default `release`, 20 samples, 8 frames. |
| `hw` | capture CPU / kernel / toolchain facts to `docs/hardware.txt`. |
| `fmt [--fix]` | `clang-format`. The bare form is the same check CI runs. |
| `tidy [preset]` | `clang-tidy` over the compile database. |
| `sysroot` | copy the container's headers to `.cache/linux-sysroot` and generate `.clangd`. Re-run after changing the base image. |
| `shell` | interactive bash in the container. |
| `down` | stop the container. Build artifacts survive. |
| `clean` | stop and drop the build volume. Next build is from scratch. |

Presets everywhere: `debug`, `release`, `asan`, `tsan`.

### Typical loops

```sh
# one module
./scripts/dev.sh test debug -R Buffer

# before committing
./scripts/dev.sh test asan
./scripts/dev.sh fmt

# server and client, two terminals
./scripts/dev.sh server              # terminal 1
./scripts/dev.sh cli PING            # terminal 2
./scripts/dev.sh bench -t set,get -n 100000 -P 16
```

## End-to-end smoke test

`tests/e2e/smoke.sh` starts the server and goes at it in three rounds. First every command
through `redis-cli`, plus expiry, a binary value, two error replies and an inline `PING`.
Then `redis-benchmark` SET and GET, unpipelined, at `-P 16`, and with 50 clients. Last,
clients that misbehave: they hang up with thousands of replies unread, get killed
mid-benchmark, or send half a command and leave.

After each round the server has to answer `PING`. At the end it has to exit 0 on SIGTERM
without a sanitizer report. The exit status is the number of failed checks.

```sh
./scripts/dev.sh smoke          # release
./scripts/dev.sh smoke asan
```

I wrote the expected replies by hand from Redis's docs, so a pass means the server agrees
with me, not with `redis-server`. The throughput lines come from one unpinned run. Don't
quote them.

## Stack sampling

`scripts/pmp.sh` is a poor man's profiler. It puts the server under `redis-benchmark`
load, stops it with gdb a set number of times, and counts identical stacks. It only needs
gdb. No `perf`, no extra container capabilities.

```sh
./scripts/dev.sh pmp                 # release, 20 samples, 8 frames deep
./scripts/dev.sh pmp asan 50 12
PMP_LOAD="-t get -c 1 -P 1" ./scripts/dev.sh pmp
```

Each sample freezes the process for tens of milliseconds. It shows where the time goes,
not how much of it. Use `perf` for the numbers.

## Things that will bite you

**Never time an asan build.** The ASan runtime in this image turns on
stack-use-after-return detection by default. Every function call then allocates a fake
stack frame, and under load the server runs about 30× slower than `release`. Pipelining
doesn't help, because the cost is per call, not per syscall.
`ASAN_OPTIONS=detect_stack_use_after_return=0` gets most of the speed back. Keep the
detection on for tests, where it catches real bugs, and take every timing from `release`.


**`build/` is a named Docker volume, not part of the bind mount.** Otherwise the macOS and
Linux `CMakeCache`s collide over `build/<preset>`, and object files on VirtioFS compile
much slower. The catch is that host `clangd` can't see the container's
`compile_commands.json` and uses the native build instead, so configure `debug` natively
at least once. Native macOS headers have no epoll, so run `sysroot` too. It generates
`.clangd` (gitignored, from `.clangd.in`), which points clangd at the container's Linux
headers so it can analyze `src/net`. `clean` discards this volume, including the FetchContent clones of
GoogleTest and pool-allocator.

**Architecture must be native.** On Apple Silicon the image must be `aarch64`. An `x86_64`
image runs under Rosetta emulation, where correctness holds but every timing is
worthless. `up` compares guest and host arch and warns. The `Dockerfile` has no
`--platform` pin for this reason, so don't add one.

**Port publishing vs. loopback.** `compose.yaml` publishes `127.0.0.1:6380`, but the host
can only reach it if the server binds `0.0.0.0`, because inside a container `127.0.0.1` is
the container's own loopback. `dev.sh server` passes `--bind 0.0.0.0` so host tools work.
For benchmarks, run the client inside the container with `dev.sh bench`, so the
measurement doesn't cross the VM's network boundary.

## Benchmarking

The host is a virtualized Linux VM on a laptop. A throughput median over many trials holds
up there. p99 is noisier, because it's a tail statistic and a median across trials
doesn't clean it. Both caveats go in `docs/benchmark.md`. Modest hardware isn't the
problem. An unstated caveat is.

Before any measurement run, quit other applications, disable Docker Desktop's Resource
Saver, and regenerate `docs/hardware.txt` on the machine that produced the numbers.
