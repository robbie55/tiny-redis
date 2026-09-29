#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/entry.h"
#include "core/table.h"
#include "support/kv.h"

// Seed with a fixed value; assert behavior, never bucket layout.
// Spill-leak checks need LSan, which only runs in the Linux container.

using tinyredis::Entry;
using tinyredis::kInlineCap;
using tinyredis::Table;
using tinyredis::key;
using tinyredis::val;

namespace {

  constexpr std::uint64_t kSeed{0x5eed'1234'abcd'0001ULL};

  // Inserts keys [from, to) with their matching values.
  void fill(Table& t, std::size_t from, std::size_t to) {
    for (std::size_t i{from}; i < to; ++i) {
      ASSERT_NE(t.insert(key(i), val(i)), nullptr) << key(i);
    }
  }

  // Runs one full scan cycle from cursor 0. The bound catches a cursor that never
  // returns to 0.
  template <typename Visit>
  void scanAll(Table& t, Visit&& visit) {
    std::size_t c{0};
    std::size_t steps{0};
    do {
      c = t.scanBucket(c, visit);
      ASSERT_LE(++steps, t.bucketCount()) << "cursor never returned to 0";
    } while (c != 0);
  }

}  // namespace

TEST(Table, RoundsInitialBucketsUpToAPowerOfTwo) {
  EXPECT_EQ(Table(0, kSeed).bucketCount(), tinyredis::kMinBuckets);
  EXPECT_EQ(Table(1, kSeed).bucketCount(), tinyredis::kMinBuckets);
  EXPECT_EQ(Table(5, kSeed).bucketCount(), 8U);
  EXPECT_EQ(Table(8, kSeed).bucketCount(), 8U);
  EXPECT_EQ(Table(1000, kSeed).bucketCount(), 1024U);
}

TEST(Table, InsertAndRemoveMoveUsedByOne) {
  Table t(4, kSeed);
  EXPECT_EQ(t.used(), 0U);
  EXPECT_EQ(t.size(), 0U);

  Entry* a{t.insert("a", "1")};
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(t.used(), 1U);
  EXPECT_EQ(t.size(), 1U);

  Entry* b{t.insert("b", "2")};
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(t.used(), 2U);
  EXPECT_EQ(t.size(), 2U);

  t.remove(a);
  EXPECT_EQ(t.used(), 1U);
  EXPECT_EQ(t.size(), 1U);
  EXPECT_EQ(t.find("a"), nullptr);
  EXPECT_EQ(t.find("b"), b);
  EXPECT_EQ(b->value(), "2");
}

// Growth happens inside insert, so used() and size() must read exactly i + 1 after the
// i-th insert, rehash or not.
TEST(Table, RehashKeepsEveryEntryAndLeavesUsedAlone) {
  Table t(4, kSeed);
  const std::size_t startBuckets{t.bucketCount()};

  std::size_t n{0};
  while (t.bucketCount() == startBuckets && n < 64) {
    ASSERT_NE(t.insert(key(n), val(n)), nullptr);
    ++n;
    ASSERT_EQ(t.used(), n);
    ASSERT_EQ(t.size(), n);
  }
  ASSERT_EQ(t.bucketCount(), startBuckets * 2) << "never grew after " << n << " inserts";

  for (std::size_t i{0}; i < n; ++i) {
    const Entry* e{t.find(key(i))};
    ASSERT_NE(e, nullptr) << key(i);
    EXPECT_EQ(e->key(), key(i));
    EXPECT_EQ(e->value(), val(i));
  }
}

TEST(Table, AssignCrossesTheInlineSpillBoundaryBothWays) {
  Table t(4, kSeed);
  const std::string k{"k"};
  Entry* e{t.insert(k, "v")};
  ASSERT_NE(e, nullptr);

  // Exactly at the cap stays inline; one over spills. Then bigger, smaller, back inline.
  const std::vector<std::string> values{
      std::string(kInlineCap - k.size(), 'a'),
      std::string(kInlineCap - k.size() + 1, 'b'),
      std::string(200, 'c'),
      std::string(50, 'd'),
      "e",
      std::string(kInlineCap, 'f'),
      "",
  };
  for (const auto& v : values) {
    t.assign(e, v);
    EXPECT_EQ(t.find(k), e) << "len=" << v.size();
    EXPECT_EQ(e->key(), k) << "len=" << v.size();
    EXPECT_EQ(e->value(), v) << "len=" << v.size();
    EXPECT_EQ(t.used(), 1U);
  }
}

// The assertion is LSan's: a leaked spill block fails the asan run in the container.
TEST(Table, DestructorFreesSpillBlocks) {
  {
    Table t(4, kSeed);
    for (std::size_t i{0}; i < 16; ++i) {
      ASSERT_NE(t.insert(key(i), std::string(100 + i, 'x')), nullptr);
    }
    Entry* grown{t.insert("grown", "small")};
    ASSERT_NE(grown, nullptr);
    t.assign(grown, std::string(300, 'y'));
    EXPECT_EQ(t.used(), 17U);
  }
  SUCCEED();
}

TEST(Table, ClearEmptiesTheTableAndItStaysUsable) {
  Table t(4, kSeed);
  fill(t, 0, 20);
  ASSERT_NE(t.insert("spilled", std::string(100, 'x')), nullptr);

  t.clear();
  EXPECT_EQ(t.used(), 0U);
  EXPECT_EQ(t.size(), 0U);
  EXPECT_EQ(t.find(key(0)), nullptr);
  EXPECT_EQ(t.find("spilled"), nullptr);

  ASSERT_NE(t.insert(key(0), "again"), nullptr);
  EXPECT_EQ(t.find(key(0))->value(), "again");
  EXPECT_EQ(t.used(), 1U);
}

TEST(Table, ScanFullCycleVisitsEveryEntryOnce) {
  constexpr std::size_t kKeys{100};
  Table t(4, kSeed);
  fill(t, 0, kKeys);

  std::map<std::string, int> seen;
  scanAll(t, [&](Entry* e) { ++seen[std::string(e->key())]; });

  ASSERT_EQ(seen.size(), kKeys);
  for (std::size_t i{0}; i < kKeys; ++i) {
    EXPECT_EQ(seen[key(i)], 1) << key(i);
  }
}

// Ticket 008's grow-only rule: a cycle that straddles a doubling visits every key present
// the whole time exactly once, and nothing twice.
TEST(Table, ScanAcrossGrowthMissesNoneAndRepeatsNone) {
  constexpr std::size_t kOriginals{3};

  for (std::size_t split{1}; split <= 4; ++split) {
    Table t(4, kSeed);
    fill(t, 0, kOriginals);
    const std::size_t startBuckets{t.bucketCount()};

    std::map<std::string, int> seen;
    auto visit{[&](Entry* e) { ++seen[std::string(e->key())]; }};

    std::size_t c{0};
    for (std::size_t i{0}; i < split && (i == 0 || c != 0); ++i) {
      c = t.scanBucket(c, visit);
    }

    std::size_t next{kOriginals};
    while (t.bucketCount() == startBuckets && next < 64) {
      ASSERT_NE(t.insert(key(next), val(next)), nullptr);
      ++next;
    }
    ASSERT_GT(t.bucketCount(), startBuckets) << "split=" << split;

    for (std::size_t steps{0}; c != 0; ++steps) {
      ASSERT_LE(steps, t.bucketCount()) << "split=" << split << " cursor never returned to 0";
      c = t.scanBucket(c, visit);
    }

    for (std::size_t i{0}; i < kOriginals; ++i) {
      EXPECT_EQ(seen[key(i)], 1) << "split=" << split << " " << key(i);
    }
    for (const auto& [k, hits] : seen) {
      EXPECT_LE(hits, 1) << "split=" << split << " " << k;
    }
  }
}

// Removes the even keys mid-scan, including chain heads and tails, and checks the walk
// neither skips nor revisits the survivors.
TEST(Table, ScanVisitorMayRemoveTheEntry) {
  constexpr std::size_t kKeys{50};
  Table t(4, kSeed);
  fill(t, 0, kKeys);

  std::map<std::string, int> seen;
  scanAll(t, [&](Entry* e) {
    const std::string k{e->key()};
    ++seen[k];
    if (std::stoul(k.substr(4)) % 2 == 0) {
      t.remove(e);
    }
  });

  ASSERT_EQ(seen.size(), kKeys);
  for (std::size_t i{0}; i < kKeys; ++i) {
    EXPECT_EQ(seen[key(i)], 1) << key(i);
    if (i % 2 == 0) {
      EXPECT_EQ(t.find(key(i)), nullptr) << key(i);
    } else {
      EXPECT_NE(t.find(key(i)), nullptr) << key(i);
    }
  }
  EXPECT_EQ(t.used(), kKeys / 2);
  EXPECT_EQ(t.size(), kKeys / 2);
}

TEST(Table, SampleSomeReturnsDistinctEntries) {
  Table t(4, kSeed);
  fill(t, 0, 100);
  std::uint64_t rng{kSeed};

  for (int round{0}; round < 50; ++round) {
    std::array<Entry*, 16> out{};
    const std::size_t got{t.sampleSome(out, rng)};
    ASSERT_GT(got, 0U) << "round=" << round;
    ASSERT_LE(got, out.size()) << "round=" << round;

    std::set<Entry*> distinct(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(got));
    EXPECT_EQ(distinct.size(), got) << "round=" << round;
    for (std::size_t i{0}; i < got; ++i) {
      ASSERT_NE(out[i], nullptr);
      EXPECT_EQ(t.find(out[i]->key()), out[i]) << "round=" << round;
    }
  }
}

TEST(Table, SampleSomeNeverReturnsMoreThanTheTableHolds) {
  Table t(4, kSeed);
  fill(t, 0, 3);
  std::uint64_t rng{kSeed};

  std::array<Entry*, 16> out{};
  const std::size_t got{t.sampleSome(out, rng)};
  EXPECT_LE(got, 3U);
  std::set<Entry*> distinct(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(got));
  EXPECT_EQ(distinct.size(), got);
}

// Buckets walked aren't observable, so make an unbounded walk obvious instead: one entry
// in 65536 buckets. A 10-bucket budget finds it about 0.015% of the time per call; a walk
// that runs until it fills `out` finds it every time.
TEST(Table, SampleSomeStopsAtTenTimesKBuckets) {
  Table t(std::size_t{1} << 16, kSeed);
  ASSERT_NE(t.insert("lonely", "v"), nullptr);
  std::uint64_t rng{kSeed};

  int hits{0};
  for (int i{0}; i < 1000; ++i) {
    std::array<Entry*, 1> out{};
    hits += static_cast<int>(t.sampleSome(out, rng));
  }
  EXPECT_LE(hits, 10);
}

TEST(Table, SampleSomeOnAnEmptyTableReturnsZero) {
  std::uint64_t rng{kSeed};
  std::array<Entry*, 5> out{};

  Table small(4, kSeed);
  EXPECT_EQ(small.sampleSome(out, rng), 0U);

  Table big(1024, kSeed);
  EXPECT_EQ(big.sampleSome(out, rng), 0U);

  small.insert("a", "1");
  small.remove(small.find("a"));
  EXPECT_EQ(small.sampleSome(out, rng), 0U);

  EXPECT_EQ(small.sampleSome(std::span<Entry*>{}, rng), 0U);
}
