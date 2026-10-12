#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/clock.h"
#include "core/entry.h"
#include "core/store.h"
#include "support/kv.h"
#include "support/oracle.h"
#include "support/zipf.h"

// Seed with a fixed value; assert behavior, never bucket layout.

using tinyredis::Entry;
using tinyredis::EvictionPolicy;
using tinyredis::key;
using tinyredis::kNoExpiry;
using tinyredis::runOracle;
using tinyredis::Store;
using tinyredis::Zipf;

namespace {

  constexpr std::uint64_t kSeed{0x5eed'1234'abcd'0004ULL};
  constexpr std::int64_t kNow{1'000};

  Store::Config capped(std::size_t maxKeys, EvictionPolicy policy) {
    return {.maxKeys = maxKeys, .seed = kSeed, .policy = policy};
  }

  // Buckets == maxKeys and K = 64: sampleSome's budget covers every bucket, so the
  // sample is the whole table and eviction is exact LRU.
  Store::Config exactLru(std::size_t maxKeys) {
    return {.initialBuckets = maxKeys,
            .maxKeys = maxKeys,
            .seed = kSeed,
            .lruSamples = 64,
            .policy = EvictionPolicy::kAllKeysLru};
  }

  // Sets keys [from, to), key i at time kNow + i so recency order is key order.
  void fill(Store& s, std::size_t from, std::size_t to, std::int64_t expireAtMs = kNoExpiry) {
    for (std::size_t i{from}; i < to; ++i) {
      const auto now{kNow + static_cast<std::int64_t>(i)};
      ASSERT_TRUE(s.set(key(i), "v", expireAtMs, now)) << key(i);
    }
  }

  bool present(Store& s, std::size_t i, std::int64_t now) {
    return s.find(key(i), now, false) != nullptr;
  }

}  // namespace

TEST(Evict, NoevictionRefusesANewKeyWhenFull) {
  Store s{capped(10, EvictionPolicy::kNoEviction)};
  fill(s, 0, 10);

  EXPECT_FALSE(s.set("new", "v", kNoExpiry, kNow + 100));

  EXPECT_EQ(s.size(), 10U);
  EXPECT_EQ(s.find("new", kNow + 100), nullptr);
  EXPECT_EQ(s.evictions(), 0U);
}

TEST(Evict, NoevictionStillAllowsOverwriteWhenFull) {
  Store s{capped(10, EvictionPolicy::kNoEviction)};
  fill(s, 0, 10);

  EXPECT_TRUE(s.set(key(3), "updated", kNoExpiry, kNow + 100));

  EXPECT_EQ(s.size(), 10U);
  const Entry* e{s.find(key(3), kNow + 100)};
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->value(), "updated");
}

TEST(Evict, AllkeysLruEvictsExactlyOnePerInsert) {
  Store s{capped(100, EvictionPolicy::kAllKeysLru)};
  fill(s, 0, 100);

  for (std::size_t i{100}; i < 150; ++i) {
    const auto now{kNow + static_cast<std::int64_t>(i)};
    ASSERT_TRUE(s.set(key(i), "v", kNoExpiry, now)) << key(i);
    ASSERT_EQ(s.size(), 100U) << key(i);
    EXPECT_TRUE(present(s, i, now)) << key(i);
  }
}

TEST(Evict, EvictionCountsInEvictions) {
  Store s{capped(100, EvictionPolicy::kAllKeysLru)};
  fill(s, 0, 100);

  // Overwrites at the cap never need a slot.
  ASSERT_TRUE(s.set(key(0), "v2", kNoExpiry, kNow + 200));
  EXPECT_EQ(s.evictions(), 0U);

  fill(s, 100, 130);
  EXPECT_EQ(s.evictions(), 30U);
  EXPECT_EQ(s.expiredKeys(), 0U);
}

TEST(Evict, ExpiredCandidateCountsAsAnExpiry) {
  Store s{exactLru(8)};
  fill(s, 0, 8, kNow + 50);

  ASSERT_TRUE(s.set("new", "v", kNoExpiry, kNow + 100));

  // The whole table is sampled and all of it is stale: reaped, none evicted.
  EXPECT_EQ(s.evictions(), 0U);
  EXPECT_EQ(s.expiredKeys(), 8U);
  EXPECT_EQ(s.size(), 1U);
}

TEST(Evict, FindWithoutTouchLeavesRecencyAlone) {
  constexpr std::int64_t kRead{kNow + 100};

  Store untouched{exactLru(8)};
  fill(untouched, 0, 8);
  ASSERT_NE(untouched.find(key(0), kRead, false), nullptr);
  ASSERT_TRUE(untouched.set("new", "v", kNoExpiry, kRead + 1));
  EXPECT_FALSE(present(untouched, 0, kRead + 1));  // still the LRU
  EXPECT_TRUE(present(untouched, 1, kRead + 1));

  // Control: the same read with touch moves key 0 off the LRU spot.
  Store touched{exactLru(8)};
  fill(touched, 0, 8);
  ASSERT_NE(touched.find(key(0), kRead), nullptr);
  ASSERT_TRUE(touched.set("new", "v", kNoExpiry, kRead + 1));
  EXPECT_TRUE(present(touched, 0, kRead + 1));
  EXPECT_FALSE(present(touched, 1, kRead + 1));
}

// Ticket 009: K=5, 16k keys, Zipf(0.99). A true-LRU oracle ranks every victim by recency;
// bad = victim in the most-recent 20%. Bar: bad rate <= 20%.
TEST(Evict, BadEvictionRateStaysWithinTwentyPercent) {
  constexpr std::size_t kKeys{16'384};
  constexpr std::size_t kUniverse{kKeys * 2};
  constexpr std::size_t kEvictions{1'000};

  Store s{capped(kKeys, EvictionPolicy::kAllKeysLru)};
  Zipf zipf{kUniverse, 0.99, kSeed};
  const auto r{runOracle(s, zipf, kUniverse, 0, kEvictions)};

  EXPECT_EQ(s.evictions(), kEvictions);
  EXPECT_LE(r.bad * 5, kEvictions) << r.bad << " of " << kEvictions << " evictions were bad";
}
