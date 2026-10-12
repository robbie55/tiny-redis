#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/bits.h"

using tinyredis::nextCursor;
using tinyredis::nextPow2;
using tinyredis::splitmix64;

namespace {

  // Buckets visited by one full cycle from cursor 0, in visit order.
  std::vector<std::size_t> fullCycle(std::size_t mask) {
    std::vector<std::size_t> order;
    std::size_t c{0};

    do {
      order.push_back(c);
      c = nextCursor(c, mask);
    } while (c != 0 && order.size() <= mask + 1);

    return order;
  }

}  // namespace

TEST(Bits, NextPow2RoundsUpAndKeepsPowersOfTwo) {
  EXPECT_EQ(nextPow2(0), 1U);
  EXPECT_EQ(nextPow2(1), 1U);
  EXPECT_EQ(nextPow2(2), 2U);
  EXPECT_EQ(nextPow2(3), 4U);
  EXPECT_EQ(nextPow2(4), 4U);
  EXPECT_EQ(nextPow2(5), 8U);
  EXPECT_EQ(nextPow2(std::size_t{1} << 18), std::size_t{1} << 18);
  EXPECT_EQ(nextPow2((std::size_t{1} << 18) + 1), std::size_t{1} << 19);
}

TEST(Bits, NextCursorVisitsEveryBucketOnceThenReturnsZero) {
  for (std::size_t buckets : {1U, 2U, 4U, 8U, 64U, 1024U}) {
    const std::size_t mask{buckets - 1};
    const auto order{fullCycle(mask)};

    ASSERT_EQ(order.size(), buckets) << "buckets=" << buckets;
    std::vector<int> hits(buckets, 0);
    for (std::size_t b : order) {
      ASSERT_LT(b, buckets) << "buckets=" << buckets;
      ++hits[b];
    }
    for (std::size_t b{0}; b < buckets; ++b) {
      EXPECT_EQ(hits[b], 1) << "buckets=" << buckets << " bucket=" << b;
    }
  }
}

// Bucket b of the small table splits into b and b | kSmall in the doubled one. A small
// bucket counts as covered if visited before the doubling, or both halves after.
TEST(Bits, NextCursorCycleSurvivesADoubling) {
  constexpr std::size_t kSmall{8};
  constexpr std::size_t kBig{kSmall * 2};

  for (std::size_t split{1}; split <= kSmall; ++split) {
    std::vector<int> before(kSmall, 0);
    std::vector<int> after(kBig, 0);

    std::size_t c{0};
    for (std::size_t i{0}; i < split && (i == 0 || c != 0); ++i) {
      ++before[c];
      c = nextCursor(c, kSmall - 1);
    }
    for (std::size_t steps{0}; c != 0 && steps <= kBig; ++steps) {
      ++after[c];
      c = nextCursor(c, kBig - 1);
    }

    for (std::size_t b{0}; b < kSmall; ++b) {
      const int halvesAfter{after[b] + after[b | kSmall]};
      if (before[b] == 1) {
        EXPECT_EQ(halvesAfter, 0) << "split=" << split << " bucket=" << b << " repeated";
      } else {
        EXPECT_EQ(before[b], 0) << "split=" << split << " bucket=" << b;
        EXPECT_EQ(after[b], 1) << "split=" << split << " bucket=" << b;
        EXPECT_EQ(after[b | kSmall], 1) << "split=" << split << " bucket=" << (b | kSmall);
      }
    }
  }
}

// Vigna's splitmix64.c from seed 0. State advances by the golden-ratio increment only;
// the mixing must not feed back into it.
TEST(Bits, Splitmix64MatchesTheReferenceSequence) {
  std::uint64_t state{0};

  EXPECT_EQ(splitmix64(state), 0xe220a8397b1dcdafULL);
  EXPECT_EQ(state, 0x9e3779b97f4a7c15ULL);
  EXPECT_EQ(splitmix64(state), 0x6e789e6aa1b965f4ULL);
  EXPECT_EQ(splitmix64(state), 0x06c45d188009454fULL);
  EXPECT_EQ(splitmix64(state), 0xf88bb8a8724c81ecULL);
  EXPECT_EQ(state, 0x78dde6e5fd29f054ULL);
}
