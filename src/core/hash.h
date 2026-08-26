#pragma once

#include <cstddef>
#include <cstdint>

namespace tinyredis {

  // Hashes key bytes. This is the hottest function in the server: it runs on every GET,
  // SET, DEL, EXISTS and EXPIRE, and again on every entry when the table rehashes.
  //
  // TODO(robbie): two things to decide.
  //   1. Throughput. Keys here are ~16-24 bytes. What does a byte-at-a-time FNV cost you
  //      versus consuming a machine word per round, and what does the 0..7 byte tail
  //      cost if you loop it?
  //   2. The seed parameter. Who supplies it, and what attack does it exist to stop?
  //      What happens to a chained table's lookup cost if an attacker can pick keys that
  //      all land in one bucket?
  std::uint64_t hashBytes(const char* p, std::size_t len, std::uint64_t seed) noexcept;

}  // namespace tinyredis
