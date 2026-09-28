#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/clock.h"
#include "core/entry.h"
#include "core/store.h"

using tinyredis::Entry;
using tinyredis::kKeepTtl;
using tinyredis::kMaxPasses;
using tinyredis::kNoExpiry;
using tinyredis::Store;

namespace {

  constexpr std::uint64_t kSeed{0x5eed'1234'abcd'0003ULL};
  constexpr std::int64_t kNow{1'000};
  constexpr std::int64_t kDeadline{kNow + 10};
  constexpr std::int64_t kLater{kNow + 100};
  // Large enough that one pass runs until the cursor wraps: a full sweep in one call.
  constexpr std::size_t kSweepAll{1'000'000};

  std::string key(std::size_t i) { return "key:" + std::to_string(i); }

  // Sets keys [from, to) expiring at expireAtMs, each one expected to succeed.
  void fill(Store& s, std::size_t from, std::size_t to, std::int64_t expireAtMs) {
    for (std::size_t i{from}; i < to; ++i) {
      ASSERT_TRUE(s.set(key(i), "v", expireAtMs, kNow)) << key(i);
    }
  }

}  // namespace

TEST(Ttl, KeyIsVisibleBeforeItsDeadline) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v", kDeadline, kNow));

  const Entry* e{s.find("k", kDeadline - 1)};
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->value(), "v");
}

TEST(Ttl, KeyIsGoneAfterItsDeadline) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v", kDeadline, kNow));

  EXPECT_EQ(s.find("k", kDeadline + 1), nullptr);
  EXPECT_EQ(s.size(), 0U);
}

TEST(Ttl, ReportsRemainingMilliseconds) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v", kDeadline, kNow));

  EXPECT_EQ(s.ttlMs("k", kNow), 10);
  EXPECT_EQ(s.ttlMs("k", kNow + 4), 6);
}

TEST(Ttl, ReportsMinusOneForPersistentKeyAndMinusTwoForMissing) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("persistent", "v", kNoExpiry, kNow));
  ASSERT_TRUE(s.set("expiring", "v", kDeadline, kNow));

  EXPECT_EQ(s.ttlMs("persistent", kLater), -1);
  EXPECT_EQ(s.ttlMs("missing", kLater), -2);
  EXPECT_EQ(s.ttlMs("expiring", kLater), -2);  // expired reads as missing
}

TEST(Ttl, SetExpireAtOnlySucceedsForLiveKeys) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("live", "v", kNoExpiry, kNow));
  ASSERT_TRUE(s.set("expired", "v", kDeadline, kNow));

  EXPECT_FALSE(s.setExpireAt("missing", kLater, kLater));
  EXPECT_FALSE(s.setExpireAt("expired", kLater + 10, kLater));
  EXPECT_TRUE(s.setExpireAt("live", kLater + 10, kLater));
  EXPECT_EQ(s.ttlMs("live", kLater), 10);
}

TEST(Ttl, PlainSetClearsAnExistingDeadline) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v1", kDeadline, kNow));
  ASSERT_TRUE(s.set("k", "v2", kNoExpiry, kNow));

  EXPECT_EQ(s.ttlMs("k", kLater), -1);
  const Entry* e{s.find("k", kLater)};
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->value(), "v2");
}

TEST(Ttl, OverwritingAnExpiredKeyStartsFresh) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "old", kDeadline, kNow));

  // KEEPTTL over an expired key has no deadline left to keep.
  ASSERT_TRUE(s.set("k", "new", kKeepTtl, kLater));

  EXPECT_EQ(s.expiredKeys(), 1U);
  EXPECT_EQ(s.ttlMs("k", kLater), -1);
  const Entry* e{s.find("k", kLater)};
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->value(), "new");
}

TEST(Ttl, ActiveExpireCycleReapsWithoutAnyAccess) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 100, kDeadline);

  const auto r{s.activeExpireCycle(kLater, kSweepAll)};

  EXPECT_EQ(r.reaped, 100U);
  EXPECT_EQ(s.size(), 0U);
  EXPECT_EQ(s.expiredKeys(), 100U);
}

TEST(Ttl, ActiveExpireCycleLeavesLiveKeysAlone) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 50, kDeadline);
  fill(s, 50, 90, kNoExpiry);
  fill(s, 90, 100, kLater);  // live at exactly its deadline

  const auto r{s.activeExpireCycle(kLater, kSweepAll)};

  EXPECT_EQ(r.reaped, 50U);
  ASSERT_EQ(s.size(), 50U);
  for (std::size_t i{50}; i < 100; ++i) {
    EXPECT_NE(s.find(key(i), kLater, false), nullptr) << key(i);
  }
}

TEST(Ttl, KeyIsLiveAtItsDeadline) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v", kDeadline, kNow));

  EXPECT_EQ(s.ttlMs("k", kDeadline), 0);
  EXPECT_NE(s.find("k", kDeadline), nullptr);
}

TEST(Ttl, PastDeadlineSetDeletesTheKey) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v", kNoExpiry, kNow));

  EXPECT_TRUE(s.set("k", "v2", kNow - 1, kNow));
  EXPECT_TRUE(s.set("absent", "v", kNow - 1, kNow));

  EXPECT_EQ(s.find("k", kNow), nullptr);
  EXPECT_EQ(s.find("absent", kNow), nullptr);
  EXPECT_EQ(s.size(), 0U);
}

TEST(Ttl, KeepTtlPreservesTheDeadline) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(s.set("k", "v1", kDeadline, kNow));
  ASSERT_TRUE(s.set("k", "v2", kKeepTtl, kNow + 2));

  EXPECT_EQ(s.ttlMs("k", kNow + 2), 8);
  const Entry* e{s.find("k", kNow + 2)};
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->value(), "v2");
  EXPECT_EQ(s.find("k", kDeadline + 1), nullptr);
}

TEST(Ttl, LazyExpiryCountsInExpiredKeys) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 3, kDeadline);

  EXPECT_EQ(s.find(key(0), kLater), nullptr);
  EXPECT_EQ(s.ttlMs(key(1), kLater), -2);
  EXPECT_FALSE(s.erase(key(2), kLater));

  EXPECT_EQ(s.expiredKeys(), 3U);
  EXPECT_EQ(s.size(), 0U);
}

TEST(Ttl, ActiveExpireCycleStopsAtMaxPasses) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 1'000, kDeadline);  // dense, all expired: every pass is over 10%

  // One sample per pass covers a handful of buckets, so kMaxPasses ends before the wrap.
  const auto r{s.activeExpireCycle(kLater, 1)};

  EXPECT_EQ(r.passes, kMaxPasses);
  EXPECT_GE(r.reaped, kMaxPasses);
  EXPECT_EQ(s.size(), 1'000U - r.reaped);
}

TEST(Ttl, ActiveExpireCycleStopsRepeatingAtTenPercent) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 1'000, kNoExpiry);

  const auto r{s.activeExpireCycle(kLater, 20)};

  EXPECT_EQ(r.passes, 1U);
  EXPECT_EQ(r.reaped, 0U);
  EXPECT_EQ(s.size(), 1'000U);
}

TEST(Ttl, TableNeverShrinksAfterMassExpiry) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 5'000, kDeadline);
  const std::size_t grown{s.bucketCount()};

  s.activeExpireCycle(kLater, kSweepAll);

  EXPECT_EQ(s.size(), 0U);
  EXPECT_EQ(s.bucketCount(), grown);
}

TEST(Ttl, ClearRestartsTheSweep) {
  Store s{{.seed = kSeed}};
  fill(s, 0, 1'000, kNoExpiry);
  // All live, so each call is one pass that parks the cursor mid-table.
  s.activeExpireCycle(kNow, 200);
  s.activeExpireCycle(kNow, 200);

  s.clear();
  fill(s, 0, 500, kDeadline);

  // A cursor left mid-table would wrap after sweeping only the rest, missing keys.
  const auto r{s.activeExpireCycle(kLater, kSweepAll)};

  EXPECT_EQ(r.reaped, 500U);
  EXPECT_EQ(s.size(), 0U);
}
