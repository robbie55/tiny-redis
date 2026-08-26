# tiny-redis — project context

## What this is

A small, polished in-memory key-value store in C++20 ("a tiny Redis"), built as a
**résumé differentiator**. It must ship as a finished, benchmarked, public GitHub repo.

**Deliverable:** a public, CI-green, `v1.0`-tagged repo at `github.com/robbie55/tiny-redis`
with a README carrying a real, measured headline number.

## The deadline is the top constraint

**Hard stop: Sunday 2026-08-30.** Immovable. Scope backward from it.

September is peak FAANG internship-application season. Robbie is a rising senior
(UNT, B.S. CS, GPA 3.91, grad Spring 2027, US citizen) targeting Summer 2027 SWE
internships. He wants this done so he can spend September on applications. Every day
this slips eats into that window.

**The single most important rule:** a finished, benchmarked GET/SET/EXPIRE store beats a
feature-rich broken one every time. When choosing between "add a feature" and "make what
exists correct + measured," **always choose the latter.**

### Cut order when a day slips

1. The one stretch feature (LRU / persistence) — cut first
2. Persistence / LRU entirely
3. Pipelining

**Never cut:** the benchmark number, or the README.

## Why this project exists (keep in view when making calls)

Robbie's general-SWE résumé gets zero traction on cold, no-referral applications. The
writing is fine — the problem is it reads as "solid full-stack student, unknown startup,
non-target school, class projects," with no instantly-legible differentiator for a
generalist screener skimming for six seconds. His genuinely elite trait (low-level C++)
is buried, and generic screeners don't weight it.

This project fixes exactly that gap, so it must stay:

- **Legible to a generalist screener**, not just a systems expert. "I built a mini-Redis
  handling X ops/sec" registers with a recruiter; "lock-free SPSC queue" doesn't. Keep the
  framing recognizable.
- **C++.** ~9 of 10 intern applicants submit a web app; a polished C++ systems/networking
  project is what makes a screener stop. **Do not switch languages.**
- **Backed by a NUMBER.** The bullet lives or dies on a metric (ops/sec, p50/p99). A
  credible benchmark is a first-class deliverable, not an afterthought.
- **A bridge across both résumés** — backend (networking, protocol, throughput) *and*
  systems (event loop, allocator, data structures).

### The résumé bullet this must earn

> **Tiny-Redis** — in-memory key-value store in C++20. Built a Redis-like KV store —
> epoll event loop, RESP protocol, TTL expiry, backed by a custom pool allocator —
> sustaining ~[X] ops/sec over TCP (measured with `redis-benchmark`), verified under
> ASan/TSan with a GoogleTest suite and CI.

Fill `[X]` with a real measured number. **Never publish a number that wasn't measured** —
this is a portfolio artifact that interviewers will open and ask about.

## How to work with Robbie

He is a **strong systems-C++ developer — treat him as a peer.** Do NOT explain RAII, move
semantics, templates, CMake, or sanitizers. He has shipped:

- A **pool allocator** (~10× faster than system alloc, 1.65 ns/op; intrusive free list over
  a cache-aligned slab; 51×100k-op median-trial harness with DCE guards + outlier
  rejection; ASan-clean, warnings-as-errors, CMake presets) — **reused here** for entry
  allocation via FetchContent from `github.com/robbie55/pool-allocator`.
- A **cycle-accurate Game Boy emulator** (SM83, GitHub Actions CI matrix).
- Pro backend experience at Sojo (AWS, Next.js, GraphQL).

He owns: modern C++, benchmark rigor, CMake, GoogleTest, sanitizers, GitHub Actions.
The gap he's closing here: **networking (sockets/epoll)** and a systems service to point at.

Spend words on the hard/novel parts — epoll event loop, RESP protocol, store design,
benchmark methodology — and on **keeping scope contained**. Build incrementally and
test-gate each stage (parser → store → commands → bench), mirroring the CPU-before-PPU
discipline from his emulator. **Keep him honest on scope and the clock every day**; if
we're behind, say so and name what to cut, in the cut order above.

## WORKING AGREEMENT — read this before writing any code

**Robbie writes the implementation. Claude does not.**

The first pass at this project was fully AI-generated and was thrown away on
2026-08-26 for exactly that reason. The point of this project is that Robbie can defend
every line of it in an interview. A repo he cannot explain is worth less than no repo.

### What Claude may do

- Scaffolding: build files, CI, presets, dotfiles, directory layout, file skeletons.
- Review code Robbie has written: correctness, edge cases, style, performance.
- Explain concepts, syscalls, protocol details, and trade-offs — in the abstract.
- Debug: read a failure, point at the cause, ask leading questions.
- Write throwaway things that never ship: scratch repro programs, one-off scripts.
- Say when we are behind schedule and what to cut.

### What Claude may NOT do

- Write or complete function bodies in `src/`, `tests/`, or `bench/`.
- "Show an example implementation" of something on the TODO list. That is the same
  thing as writing it.
- Fill in a `TODO(robbie)` block.
- Hand over a full algorithm when asked how something works. Explain the shape, the
  constraints, and the trade-off; let Robbie write it.

If Robbie explicitly asks for an implementation, say what this file says, then do it if
he confirms. Default is always: he drives.

### The stub convention

Every unwritten function calls `unimplemented()` from `src/core/todo.h` and aborts at
runtime. This keeps the tree building and linking at all times, so any one finished
module can be built and tested without the rest existing. Delete `todo.h` when the last
stub is gone.

Test bodies are `FAIL() << "TODO(robbie): not written yet"`. Test *names* are a coverage
checklist; an unwritten test must never read as green.

### Honesty rules (non-negotiable)

- **No number goes in the README that was not measured.** The discarded first pass
  published invented benchmark figures (`723.6k ops/sec`, fabricated p50s, a fake
  redis-server comparison). That is disqualifying if caught, and it would be caught.
- Every published number traces to a committed results file, median of N trials after
  warmup, with p50 and p99, and states the hardware and flags.
- Commit history should reflect real incremental work.

## Spec

### Core (must-have — this is the project even if nothing below the line lands)

- **epoll TCP server / event loop** — single-threaded, non-blocking sockets, accept + read
  + write, many concurrent clients on one thread.
- **RESP parser** — enough to speak to real clients: `redis-cli` and `redis-benchmark`.
  Support both inline and array/bulk-string command forms.
- **Commands:** `GET`, `SET`, `DEL`, `EXISTS`, `EXPIRE`/`TTL`, `PING`. These are plenty.
- **Store:** open-addressing or chained hash map; entries allocated through the pool
  allocator. Keys and values are binary-safe byte strings.
- **TTL expiry:** lazy expiry on access + a simple periodic sweep.

### Stretch — pick EXACTLY ONE, only if core is done and solid (do NOT do both)

1. **LRU eviction** with a max-memory/max-keys cap (intrusive doubly-linked list + map), or
2. **Append-only persistence** (log commands to a file, replay on startup).

### Benchmark (a deliverable, not optional)

- Drive with `redis-benchmark` (it speaks RESP) and/or a small pipelined C++ client.
- Report **throughput (ops/sec) and latency p50/p99 — not averages.**
- Reuse the allocator's methodology: warmup, median trials, DCE guards, outlier rejection.
- Pipelining on/off is a valuable comparison.
- Land a credible headline number for the README and the résumé.

## Conventions (match his existing repos)

- **C++20**, CMake presets (`debug`/`release`/`asan`/`tsan`), FetchContent for deps,
  GoogleTest.
- **Warnings-as-errors:** `-Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic`.
- **ASan and TSan clean.** clang-format + clang-tidy gate.
- **GitHub Actions CI** like the emulator's.
- **Platform:** epoll is Linux-only. Target Linux for the server, but keep the store and
  RESP layers portable so macOS CI and unit tests still build/test the non-networking code.
  **Do NOT burn time abstracting epoll/kqueue cross-platform** — that is scope creep. A
  Linux-only event loop with portable, unit-tested internals is the right call for 5 days.
  (Robbie's dev machine is macOS, so the server can only be built/benchmarked on Linux.)
- **Benchmark rigor is part of the deliverable**, as in the allocator.
- **README is a first-class artifact:** architecture diagram, benchmark table with the
  headline number, RESP/command list, build instructions, one-line pitch.

## Layout

```
src/core/   portable: buffer, resp, command, store, entry, hash, clock, todo.h
src/net/    Linux-only: event_loop, connection, server  (epoll lives here)
src/main.cpp            KEPT AS SCAFFOLDING (argv parsing, signals)
src/core/clock.h        KEPT AS SCAFFOLDING
tests/      GoogleTest: buffer, resp, store, ttl, commands  (names only, bodies FAIL)
bench/      bench_client.cpp — stub; the harness is a deliverable
docs/
```

Everything not marked KEPT AS SCAFFOLDING is a stub for Robbie to implement.
Headers carry declarations and TODO design questions; they deliberately do NOT carry the
answers.
