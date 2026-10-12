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
      {.len = 1, .want = 0x68A914128E01E473ULL},  {.len = 7, .want = 0x2F098AB0C751325AULL},
      {.len = 8, .want = 0xEAD411E67EBE2EEAULL},  {.len = 9, .want = 0x75927F9D95124362ULL},
      {.len = 15, .want = 0xF30EB725BB91C9EAULL}, {.len = 16, .want = 0x8972188433A5C5B7ULL},
      {.len = 24, .want = 0x31185A47AF932F3AULL}, {.len = 63, .want = 0x385D3E39E5F37359ULL},
  };
  for (const auto& c : kCases) {
    const std::string m{ramp(c.len)};
    EXPECT_EQ(sipHash13(m.data(), m.size(), 0, 0), c.want) << "len " << c.len;
  }
  const std::string key{"user:1000:session"};
  EXPECT_EQ(sipHash13(key.data(), key.size(), 0, 0), 0xF3AEE0B115FA8CB6ULL);
}

TEST(Hash, EmptyInputIsWellDefined) { EXPECT_EQ(hashBytes(nullptr, 0, 42), hashBytes("", 0, 42)); }

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
