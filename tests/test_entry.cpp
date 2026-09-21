#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/clock.h"
#include "core/entry.h"

using tinyredis::Entry;
using tinyredis::kBenchKeyLen;
using tinyredis::kBenchValLen;
using tinyredis::kInlineCap;
using tinyredis::kNoExpiry;

// The pool hands out raw storage and the store frees it without running a destructor, so
// Entry must stay trivial. A user-declared special member breaks this.
static_assert(std::is_trivially_copyable_v<Entry>);
static_assert(std::is_trivially_destructible_v<Entry>);

namespace {

  std::uint32_t len(std::string_view s) { return static_cast<std::uint32_t>(s.size()); }

  // Fills an entry the way the store will for a pair that fits inline.
  Entry makeInline(std::string_view key, std::string_view val) {
    Entry e{};
    e.expiresAtMs = kNoExpiry;
    e.keyLen = len(key);
    e.valLen = len(val);
    std::memcpy(e.inlineBuf.data(), key.data(), key.size());
    std::memcpy(e.inlineBuf.data() + key.size(), val.data(), val.size());
    return e;
  }

  // Fills an entry the way the store will for a spilled pair. `block` stands in for the
  // new[] allocation and must outlive the entry.
  Entry makeSpilled(std::string_view key, std::string_view val, std::vector<char>& block) {
    block.assign(key.begin(), key.end());
    block.insert(block.end(), val.begin(), val.end());
    Entry e{};
    e.expiresAtMs = kNoExpiry;
    e.keyLen = len(key);
    e.valLen = len(val);
    e.bufPtr = block.data();
    return e;
  }

}  // namespace

TEST(Entry, BenchPairIsInline) {
  const std::string key(kBenchKeyLen, 'k');
  const std::string val(kBenchValLen, 'v');
  const Entry e{makeInline(key, val)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
  EXPECT_EQ(e.key().data(), e.inlineBuf.data());
}

TEST(Entry, PairOfExactlyInlineCapStaysInline) {
  const std::string key(10, 'k');
  const std::string val(kInlineCap - 10, 'v');
  const Entry e{makeInline(key, val)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
  EXPECT_EQ(e.value().data(), e.inlineBuf.data() + key.size());
}

TEST(Entry, PairOneByteOverInlineCapReadsTheSpillBlock) {
  const std::string key(10, 'k');
  const std::string val(kInlineCap - 10 + 1, 'v');
  std::vector<char> block;
  const Entry e{makeSpilled(key, val, block)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
  EXPECT_EQ(e.key().data(), block.data());
  EXPECT_EQ(e.value().data(), block.data() + key.size());
}

TEST(Entry, LongKeySpillsWithItsValue) {
  const std::string key(200, 'k');
  const std::string val{"xxx"};
  std::vector<char> block;
  const Entry e{makeSpilled(key, val, block)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
}

TEST(Entry, EmptyKeyAndValue) {
  const Entry e{makeInline("", "")};

  EXPECT_TRUE(e.key().empty());
  EXPECT_TRUE(e.value().empty());
}

TEST(Entry, InlineBytesAreBinarySafe) {
  using namespace std::string_view_literals;
  const std::string_view key{"a\0b\r\n"sv};
  const std::string_view val{"\0\0\xff"sv};
  const Entry e{makeInline(key, val)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
}

TEST(Entry, SpilledBytesAreBinarySafe) {
  std::string key(40, '\0');
  key[20] = '\n';
  std::string val(40, '\xff');
  val[0] = '\0';
  std::vector<char> block;
  const Entry e{makeSpilled(key, val, block)};

  EXPECT_EQ(e.key(), key);
  EXPECT_EQ(e.value(), val);
}

TEST(Entry, PersistentKeyNeverExpires) {
  Entry e{makeInline("k", "v")};
  e.expiresAtMs = kNoExpiry;

  EXPECT_FALSE(e.expired(0));
  EXPECT_FALSE(e.expired(kNoExpiry));
  EXPECT_FALSE(e.expired(std::numeric_limits<std::int64_t>::max()));
}

TEST(Entry, ExpiresAtItsDeadlineNotBefore) {
  Entry e{makeInline("k", "v")};
  e.expiresAtMs = 1000;

  EXPECT_FALSE(e.expired(999));
  EXPECT_FALSE(e.expired(1000));
  EXPECT_TRUE(e.expired(1001));
}

TEST(Entry, DeadlineAtZeroIsPersistent) {
  Entry e{makeInline("k", "v")};
  e.expiresAtMs = 0;  // use kNoExpiry to represent no expiration

  EXPECT_FALSE(e.expired(0));
}
