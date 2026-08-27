# Linux build/dev/bench environment for tiny-redis.
#
# epoll is Linux-only, so src/net and every benchmark number come from in here. The
# portable core (src/core) still builds and tests natively on macOS -- use that for the
# fast unit-test loop and this for anything touching a socket.
#
# No --platform pin on purpose: the image builds for the host architecture. On Apple
# Silicon that is linux/arm64 running natively under Virtualization.framework. Forcing
# linux/amd64 here would silently drag in Rosetta emulation and make every measurement
# worthless. `./scripts/dev.sh up` checks for that and warns.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# build-essential -> gcc-13/g++-13, which is C++20-complete.
# cmake on 24.04 is 3.28, satisfying our cmake_minimum_required(3.25).
# redis-tools -> redis-cli and redis-benchmark, the T-11 gate and a T-13 data source.
# clang-format/clang-tidy match the CI lint jobs so formatting fails here, not on CI.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      ccache \
      clang \
      clang-format \
      clang-tidy \
      cmake \
      curl \
      gdb \
      git \
      ninja-build \
      pkg-config \
      redis-tools \
      strace \
      tmux \
      vim \
    && rm -rf /var/lib/apt/lists/*

# FetchContent clones googletest and pool-allocator over https.
RUN git config --global --add safe.directory /work

ENV CCACHE_DIR=/root/.ccache
ENV CMAKE_C_COMPILER_LAUNCHER=ccache
ENV CMAKE_CXX_COMPILER_LAUNCHER=ccache

WORKDIR /work

CMD ["sleep", "infinity"]
