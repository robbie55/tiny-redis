#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "core/hash.h"

using tinyredis::hashBytes;
using tinyredis::sipHash13;

namespace {
  // Message is bytes 0, 1, ..., n-1.
  std::string ramp(std::size_t n) {
    std::string s(n, '\0');
    for (std::size_t i{0}; i < n; ++i) {
      s[i] = static_cast<char>(i);
    }
    return s;
  }
}  // namespace

// Zero-key SipHash-1-3, cross-checked against CPython (PYTHONHASHSEED=0 hash(bytes)) and
// Rust's DefaultHasher. Lengths cover every tail branch plus whole-word boundaries.
TEST(Hash, SipHash13MatchesReferenceVectors) {
  struct Case {
    std::size_t len;
    std::uint64_t want;
  };
  constexpr Case kCases[]{
      {1, 0x68a914128e01e473ULL},  {7, 0x2f098ab0c751325aULL},  {8, 0xead411e67ebe2eeaULL},
      {9, 0x75927f9d95124362ULL},  {15, 0xf30eb725bb91c9eaULL}, {16, 0x8972188433a5c5b7ULL},
      {24, 0x31185a47af932f3aULL}, {63, 0x385d3e39e5f37359ULL},
  };
  for (const auto& c : kCases) {
    const std::string m{ramp(c.len)};
    EXPECT_EQ(sipHash13(m.data(), m.size(), 0, 0), c.want) << "len " << c.len;
  }
  const std::string key{"user:1000:session"};
  EXPECT_EQ(sipHash13(key.data(), key.size(), 0, 0), 0xf3aee0b115fa8cb6ULL);
}

TEST(Hash, EmptyInputIsWellDefined) {
  EXPECT_EQ(hashBytes(nullptr, 0, 42), hashBytes("", 0, 42));
}

// The length byte in the final block is what separates these.
TEST(Hash, TrailingZeroBytesChangeTheHash) {
  const char m[]{'a', 'b', '\0'};
  EXPECT_NE(hashBytes(m, 2, 1), hashBytes(m, 3, 1));
}

TEST(Hash, SeedChangesTheHash) {
  const std::string key{"user:1000:session"};
  EXPECT_NE(hashBytes(key.data(), key.size(), 1), hashBytes(key.data(), key.size(), 2));
  EXPECT_EQ(hashBytes(key.data(), key.size(), 1), hashBytes(key.data(), key.size(), 1));
}

// Buckets mask the low bits, so sequential keys must spread across them.
TEST(Hash, LowBitsSpreadSequentialKeys) {
  constexpr std::size_t kBuckets{1024};
  std::unordered_set<std::uint64_t> used;
  for (int i{0}; i < 4096; ++i) {
    const std::string key{"key:" + std::to_string(i)};
    used.insert(hashBytes(key.data(), key.size(), 7) & (kBuckets - 1));
  }
  // 4096 balls into 1024 bins leaves ~1.8% empty on average.
  EXPECT_GT(used.size(), kBuckets * 95 / 100);
}
