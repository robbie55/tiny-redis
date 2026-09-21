#pragma once

#include <cstddef>
#include <cstdint>

namespace tinyredis {

  // Smallest power of two >= n; 1 for n == 0.
  std::size_t nextPow2(std::size_t n) noexcept;

  // Reverse-binary increment over a power-of-two table (mask = buckets - 1): bumps the
  // high bits first, so a cycle started before a doubling still visits every bucket once.
  // Returns 0 when the cycle ends.
  std::size_t nextCursor(std::size_t cursor, std::size_t mask) noexcept;

  // Advances state and returns the next output. Its low bits are good, unlike
  // xoroshiro128+, which matters because bucket indexing masks them (ticket 006).
  std::uint64_t splitmix64(std::uint64_t& state) noexcept;

}  // namespace tinyredis
