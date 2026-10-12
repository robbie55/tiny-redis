#pragma once

#include <cstddef>
#include <cstdint>

namespace tinyredis {

  // Hashes key bytes. Runs on every GET, SET, DEL, EXISTS and EXPIRE, and on every entry
  // during a rehash, which makes it the hottest function in the server.
  //
  // SipHash-1-3 keyed from seed, as Redis does. A secret key stops hash flooding: without
  // it a client cannot pick keys that chain into one bucket and turn lookups O(n).
  // Multiply-based word hashes (Murmur, FNV) fail here: they have seed-independent
  // collisions.
  std::uint64_t hashBytes(const char* p, std::size_t len, std::uint64_t seed) noexcept;

  // Plain SipHash-1-3 with a 128-bit key (k0, k1). Exposed for reference-vector tests.
  std::uint64_t sipHash13(const char* p, std::size_t len, std::uint64_t k0,
                          std::uint64_t k1) noexcept;

}  // namespace tinyredis
