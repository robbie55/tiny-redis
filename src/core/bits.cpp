#include "core/bits.h"

#include <bit>
#include <cassert>

namespace {
  // Reverses the low bits of v that mask covers. mask is all-ones (buckets - 1).
  std::size_t reverseBits(std::size_t v, std::size_t mask) noexcept {
    std::size_t out{0};
    for (std::size_t m{mask}; m != 0; m >>= 1) {
      out = (out << 1) | (v & 1);
      v >>= 1;
    }
    return out;
  }
}  // namespace

namespace tinyredis {

  std::size_t nextPow2(std::size_t n) noexcept {
    if (n == 0) {
      return 1;
    }

    return std::bit_ceil(n);
  }

  std::size_t nextCursor(std::size_t cursor, std::size_t mask) noexcept {
    const std::size_t r{reverseBits(cursor, mask) + 1};

    // cycle set back to 0, mask = buckets - 1
    if (r > mask) {
      return 0;
    }

    return reverseBits(r, mask);
  }

  std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    state += 0x9e3779b97f4a7c15;  // golden ratio incr
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;

    return z ^ (z >> 31);
  }

}  // namespace tinyredis
