#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/clock.h"
#include "core/entry.h"
#include "core/store.h"
#include "core/table.h"
#include "support/kv.h"

// Seed Store::Config with a fixed value so bucket layout is the same every run.
// Entry* dies at the next Store call, so read key()/value() before calling again.

using namespace std::string_view_literals;

using tinyredis::Entry;
using tinyredis::key;
using tinyredis::kInlineCap;
using tinyredis::kNoExpiry;
using tinyredis::kPoolCapacity;
using tinyredis::Store;
using tinyredis::val;

namespace {

  constexpr std::uint64_t kSeed{0x5eed'1234'abcd'0002ULL};
  constexpr std::int64_t kNow{1'000};

  bool put(Store& s, std::string_view k, std::string_view v) {
    return s.set(k, v, kNoExpiry, kNow);
  }

  // Sets keys [from, to) with their matching values, each one expected to succeed.
  void fill(Store& s, std::size_t from, std::size_t to) {
    for (std::size_t i{from}; i < to; ++i) {
      ASSERT_TRUE(put(s, key(i), val(i))) << key(i);
    }
  }

  void expectValue(Store& s, std::string_view k, std::string_view v) {
    const Entry* e{s.find(k, kNow)};
    ASSERT_NE(e, nullptr) << k;
    EXPECT_EQ(e->key(), k);
    EXPECT_EQ(e->value(), v) << k;
  }

}  // namespace

TEST(Store, SetThenFind) {
  Store s{{.seed = kSeed}};
  EXPECT_TRUE(put(s, "k", "v"));
  EXPECT_EQ(s.size(), 1U);
  expectValue(s, "k", "v");
}

TEST(Store, MissingKeyReturnsNull) {
  Store s{{.seed = kSeed}};
  EXPECT_EQ(s.find("nope", kNow), nullptr);

  ASSERT_TRUE(put(s, "a", "1"));
  EXPECT_EQ(s.find("b", kNow), nullptr);
  EXPECT_EQ(s.find("aa", kNow), nullptr);  // prefix of neither, same first byte
  EXPECT_EQ(s.find("", kNow), nullptr);
}

TEST(Store, OverwriteReplacesValueWithoutGrowingSize) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(put(s, "k", "first"));
  ASSERT_TRUE(put(s, "k", "second"));
  EXPECT_EQ(s.size(), 1U);
  expectValue(s, "k", "second");
}

// Distinct fill bytes per step, so a stale tail from the previous value shows up.
TEST(Store, ShrinkingOverwriteKeepsCorrectLength) {
  Store s{{.seed = kSeed}};
  const std::string big(static_cast<std::size_t>(kInlineCap * 4), 'a');
  const std::string smallerSpilled(static_cast<std::size_t>(kInlineCap * 2), 'b');
  const std::string tiny{"c"};

  ASSERT_TRUE(put(s, "k", big));
  expectValue(s, "k", big);

  ASSERT_TRUE(put(s, "k", smallerSpilled));  // spilled -> smaller spilled
  expectValue(s, "k", smallerSpilled);

  ASSERT_TRUE(put(s, "k", tiny));  // spilled -> inline
  expectValue(s, "k", tiny);

  ASSERT_TRUE(put(s, "k", ""));  // inline -> shorter inline
  expectValue(s, "k", "");

  ASSERT_TRUE(put(s, "k", big));  // and back out to spilled
  expectValue(s, "k", big);
  EXPECT_EQ(s.size(), 1U);
}

TEST(Store, KeysAndValuesAreBinarySafe) {
  Store s{{.seed = kSeed}};
  const auto k1{"a\0b"sv};
  const auto k2{"a\0c"sv};
  const auto v1{"\0\xff\0"sv};
  const auto v2{"\r\n\0"sv};

  ASSERT_TRUE(put(s, k1, v1));
  ASSERT_TRUE(put(s, k2, v2));
  EXPECT_EQ(s.size(), 2U);

  expectValue(s, k1, v1);
  expectValue(s, k2, v2);
  EXPECT_EQ(s.find("a"sv, kNow), nullptr);  // stops at the NUL a C string would
}

TEST(Store, SpillsValuesLargerThanTheInlineBuffer) {
  Store s{{.seed = kSeed}};
  const std::string longKey(kInlineCap, 'k');  // spills with any non-empty value
  const std::string bigVal(static_cast<std::size_t>(kInlineCap * 8), 'v');

  ASSERT_TRUE(put(s, "short", bigVal));
  ASSERT_TRUE(put(s, longKey, "x"));
  ASSERT_TRUE(put(s, longKey + "2", bigVal));

  expectValue(s, "short", bigVal);
  expectValue(s, longKey, "x");
  expectValue(s, longKey + "2", bigVal);
}

TEST(Store, EraseRemovesTheKey) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(put(s, "a", "1"));
  ASSERT_TRUE(
      put(s, "b",
          std::string(static_cast<std::size_t>(kInlineCap * 2), 'b')));  // spilled erase path too

  EXPECT_TRUE(s.erase("a", kNow));
  EXPECT_EQ(s.find("a", kNow), nullptr);
  EXPECT_EQ(s.size(), 1U);
  EXPECT_FALSE(s.erase("a", kNow));  // DEL on a missing key -> 0
  EXPECT_FALSE(s.erase("zz", kNow));

  EXPECT_TRUE(s.erase("b", kNow));
  EXPECT_EQ(s.size(), 0U);
}

TEST(Store, GrowsAndKeepsEveryKeyReachable) {
  Store s{{.initialBuckets = 4, .seed = kSeed}};
  const std::size_t start{s.bucketCount()};
  constexpr std::size_t kN{1'000};

  fill(s, 0, kN);
  EXPECT_GT(s.bucketCount(), start);
  EXPECT_EQ(s.size(), kN);
  for (std::size_t i{0}; i < kN; ++i) {
    expectValue(s, key(i), val(i));
  }
}

TEST(Store, ClearEmptiesTheTable) {
  Store s{{.initialBuckets = 4, .seed = kSeed}};
  fill(s, 0, 100);
  const std::size_t buckets{s.bucketCount()};

  s.clear();
  EXPECT_EQ(s.size(), 0U);
  EXPECT_EQ(s.bucketCount(), buckets);  // grow-only, clear included
  for (std::size_t i{0}; i < 100; ++i) {
    EXPECT_EQ(s.find(key(i), kNow), nullptr) << key(i);
  }

  fill(s, 0, 10);  // still usable
  expectValue(s, key(3), val(3));
}

TEST(Store, RefusesToExceedMaxKeys) {
  constexpr std::size_t kCap{8};
  Store s{{.maxKeys = kCap, .seed = kSeed}};  // default policy: noeviction
  fill(s, 0, kCap);

  EXPECT_FALSE(put(s, key(kCap), val(kCap)));
  EXPECT_EQ(s.size(), kCap);
  EXPECT_EQ(s.find(key(kCap), kNow), nullptr);

  EXPECT_TRUE(put(s, key(0), "overwrite"));  // no slot needed
  expectValue(s, key(0), "overwrite");

  ASSERT_TRUE(s.erase(key(1), kNow));  // a freed slot is reusable
  EXPECT_TRUE(put(s, key(kCap), val(kCap)));
  EXPECT_EQ(s.size(), kCap);
}

TEST(Store, EmptyKeyAndEmptyValueAreLegal) {
  Store s{{.seed = kSeed}};
  ASSERT_TRUE(put(s, "", ""));
  ASSERT_TRUE(put(s, "k", ""));
  EXPECT_EQ(s.size(), 2U);
  expectValue(s, "", "");
  expectValue(s, "k", "");

  ASSERT_TRUE(put(s, "", "v"));
  expectValue(s, "", "v");
  EXPECT_TRUE(s.erase("", kNow));
  EXPECT_EQ(s.find("", kNow), nullptr);
}

// A set maxKeys presizes the table so filling to the cap never rehashes.
TEST(Store, PreallocatedTableNeverGrows) {
  constexpr std::size_t kCap{100};
  Store s{{.initialBuckets = 4, .maxKeys = kCap, .seed = kSeed}};
  const std::size_t buckets{s.bucketCount()};
  EXPECT_GE(buckets, kCap);

  fill(s, 0, kCap);
  EXPECT_EQ(s.bucketCount(), buckets);
}

TEST(Store, MaxKeysZeroMeansPoolCapacity) {
  EXPECT_EQ(Store({.maxKeys = 0, .seed = kSeed}).maxKeys(), kPoolCapacity);
  EXPECT_EQ(Store({.maxKeys = 10, .seed = kSeed}).maxKeys(), 10U);
  EXPECT_EQ(Store({.maxKeys = kPoolCapacity, .seed = kSeed}).maxKeys(), kPoolCapacity);
}

TEST(Store, FillsTheWholePool) {
  Store s{{.seed = kSeed}};
  fill(s, 0, kPoolCapacity);
  EXPECT_EQ(s.size(), kPoolCapacity);

  EXPECT_FALSE(put(s, "one-too-many", "x"));
  EXPECT_EQ(s.size(), kPoolCapacity);

  expectValue(s, key(0), val(0));
  expectValue(s, key(kPoolCapacity - 1), val(kPoolCapacity - 1));
}

TEST(Store, ZeroSeedStillWorks) {
  Store s{{.initialBuckets = 4, .seed = 0}};
  constexpr std::size_t kN{500};
  fill(s, 0, kN);
  EXPECT_EQ(s.size(), kN);
  for (std::size_t i{0}; i < kN; ++i) {
    expectValue(s, key(i), val(i));
  }
}
