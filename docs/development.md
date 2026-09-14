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
| `hw` | capture CPU / kernel / toolchain facts to `docs/hardware.txt`. |
| `fmt [--fix]` | `clang-format`. The bare form is the same check CI runs. |
| `tidy [preset]` | `clang-tidy` over the compile database. |
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

## Things that will bite you

**Unwritten code fails on purpose.** Unwritten test bodies are `FAIL() << "TODO(robbie)"`
and unwritten functions call `unimplemented()`, which aborts. `server`, `cli` and `bench`
abort until the net layer and command dispatch exist.

**`build/` is a named Docker volume, not part of the bind mount.** Otherwise the macOS and
Linux `CMakeCache`s collide over `build/<preset>`, and object files on VirtioFS compile
much slower. The catch is that host `clangd` can't see the container's
`compile_commands.json` and uses the native build instead, so configure `debug` natively
at least once. `clean` discards this volume, including the FetchContent clones of
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
