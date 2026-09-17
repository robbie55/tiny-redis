#pragma once

#include <cstddef>
#include <cstdint>

namespace tinyredis {

  // Hashes key bytes. Runs on every GET, SET, DEL, EXISTS and EXPIRE, and on every entry
  // during a rehash, which makes it the hottest function in the server.
  //
  // TODO(robbie): two decisions.
  //   1. Throughput. Keys here are ~16-24 bytes. What does byte-at-a-time FNV cost
  //      against one machine word per round, and what does looping the 0-7 byte tail
  //      cost?
  //   2. The seed. Who supplies it, and what attack does it stop? What happens to a
  //      chained table's lookups if an attacker picks keys that all land in one bucket?
  std::uint64_t hashBytes(const char* p, std::size_t len, std::uint64_t seed) noexcept;

}  // namespace tinyredis
