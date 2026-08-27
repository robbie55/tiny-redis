# Development

Two build environments, on purpose.

| | where | what it builds | use it for |
|---|---|---|---|
| **native macOS** | your laptop | `tinyredis_core` + tests | the fast inner loop on `src/core` |
| **Docker (Linux)** | `scripts/dev.sh` | everything, incl. `tiny-redis-server` | sockets, ASan/TSan, benchmarks |

`epoll` is Linux-only, so `src/net` and every benchmark number come from the container.
`src/core` is deliberately free of any networking dependency and builds natively on
macOS — which is both faster to iterate on and a standing check that the store and
protocol layers haven't grown a dependency on the event loop.

## Native (macOS)

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, `asan`, `tsan`. On macOS these configure the portable core
and its tests only; CMake prints a status line saying so.

## Container (Linux)

Requires Docker Desktop to be running. Everything goes through one script:

```sh
./scripts/dev.sh help
```

### First run

```sh
./scripts/dev.sh up          # builds the image (~2-3 min), starts the container
./scripts/dev.sh test debug  # proves toolchain + FetchContent + CTest end to end
./scripts/dev.sh hw          # writes docs/hardware.txt -- commit it
```

`up` is idempotent; every other command calls it for you. The container is long-lived
and `dev.sh` `exec`s into it, so commands after the first cost no container-start time.

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
| `fmt [--fix]` | `clang-format`; bare form is the same check CI runs. |
| `tidy [preset]` | `clang-tidy` over the compile database. |
| `shell` | interactive bash in the container. |
| `down` | stop the container. Build artifacts survive. |
| `clean` | stop **and** drop the build volume. Next build is from scratch. |

Presets everywhere: `debug`, `release`, `asan`, `tsan`.

### Typical loops

```sh
# working a single module
./scripts/dev.sh test debug -R Buffer

# before committing -- same gates as CI
./scripts/dev.sh test asan
./scripts/dev.sh fmt

# Stage 3, two terminals
./scripts/dev.sh server              # terminal 1
./scripts/dev.sh cli PING            # terminal 2
./scripts/dev.sh bench -t set,get -n 100000 -P 16
```

## Things that will bite you

**Tests failing is currently correct.** Every test body is `FAIL() << "TODO(robbie)"`
and every function body calls `unimplemented()`. A green suite today would mean
something is wrong. Likewise `server`/`cli`/`bench` abort until T-09..T-11 land.

**`build/` is a named Docker volume, not part of the bind mount.** The macOS and Linux
`CMakeCache`s would otherwise collide over `build/<preset>`, and keeping object files
off VirtioFS is a large compile-time win. Consequence: the container's
`compile_commands.json` is invisible to the host, so host `clangd` uses the native
build — configure `debug` natively at least once. `clean` discards this volume, which
also discards the FetchContent clones of GoogleTest and pool-allocator.

**Architecture must be native.** On Apple Silicon the image must be `aarch64`. If it
builds `x86_64` it is running under Rosetta emulation: correctness still holds, but
every timing is worthless. `up` compares guest and host arch and warns. The `Dockerfile`
carries no `--platform` pin for exactly this reason — don't add one.

**Port publishing vs. loopback.** `compose.yaml` publishes `127.0.0.1:6380`, but reaching
it from the host needs the server bound to `0.0.0.0` — inside a container, `127.0.0.1`
is the container's own loopback. `dev.sh server` passes `--bind 0.0.0.0` so host tools
work. **For benchmarks, run the client inside the container** (`dev.sh bench`) so the
measurement doesn't cross the VM's network boundary.

## Benchmarking

The host is a virtualized Linux VM on a laptop. That is fine for throughput measured as
a median over many trials, and noisier for p99, which is itself a tail statistic and
can't be medianed clean. Both facts belong in `docs/benchmark.md` — see the rules there.
Stating the host honestly is what makes the number defensible; the hardware being
modest is not the problem, an unstated caveat is.

Before any measurement run: quit other applications, disable Docker Desktop's Resource
Saver, and regenerate `docs/hardware.txt` on the machine that produced the numbers.
