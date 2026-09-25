#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "PoolAllocator.h"
#include "core/bits.h"
#include "core/entry.h"
#include "core/todo.h"

namespace tinyredis {

  // Pool block count. PoolAllocator carves one aligned slab up front and never grows, so
  // this is a hard cap on live keys.
  //
  // TODO(robbie): pick a number and know its cost, since slab bytes = capacity *
  // sizeof(Entry).
#ifndef TINYREDIS_POOL_CAPACITY
#define TINYREDIS_POOL_CAPACITY (1u << 18)
#endif
  inline constexpr std::size_t kPoolCapacity{TINYREDIS_POOL_CAPACITY};

  using EntryPool = PoolAllocator<Entry, kPoolCapacity>;

  inline constexpr std::size_t kMinBuckets{4};

  // Chained hash table of entries. Owns their memory, spill blocks included, and knows
  // nothing about deadlines, recency or policy. Grow-only: shrinking is what makes the
  // scan cursor revisit entries (ticket 002).
  class Table {
   public:
    // initialBuckets rounds up to a power of two, at least kMinBuckets.
    Table(std::size_t initialBuckets, std::uint64_t hashSeed);

    // Frees spill blocks first; the pool's destructor only drops the slab.
    ~Table();

    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
    Table(Table&&) = delete;
    Table& operator=(Table&&) = delete;

    Entry* find(std::string_view key) noexcept;

    // Key must be absent. nullptr only when the pool is empty, which Store's maxKeys cap
    // makes a bug. May grow the table.
    Entry* insert(std::string_view key, std::string_view value);

    // New value in place, crossing the inline/spilled boundary either way.
    void assign(Entry* e, std::string_view value);

    // The only way an entry's memory is freed.
    void remove(Entry* e) noexcept;

    void clear() noexcept;

    // Visits every entry chained at `cursor`, then returns the reverse-binary next cursor;
    // 0 means a full cycle ended. `visit(Entry*)` may remove the entry it was handed.
    template <typename Visit>
    std::size_t scanBucket(std::size_t cursor, Visit visit);

    // dictGetSomeKeys walk: random start bucket, whole chains, stops at out.size() entries
    // or out.size() * 10 buckets. Returns entries written. Distinct, possibly expired.
    std::size_t sampleSome(std::span<Entry*> out, std::uint64_t& rngState) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t bucketCount() const noexcept;
    [[nodiscard]] std::size_t used() const noexcept;

   private:
    void maybeGrow();
    void rehash(std::size_t newBucketCount);
    [[nodiscard]] std::size_t bucketOf(std::uint32_t h) const noexcept;

    // TODO(robbie): bucket array, pool, live count, hash seed.
    std::vector<Entry*> buckets_;
    EntryPool pool_;
    std::size_t liveCount_{0};
    std::uint64_t hashSeed_;
  };

  template <typename Visit>
  std::size_t Table::scanBucket(std::size_t cursor, Visit visit) {
    const std::size_t mask{bucketCount() - 1};
    for (Entry* e{buckets_[cursor & mask]}; e != nullptr;) {
      Entry* next{e->next};  // visit may remove e
      visit(e);
      e = next;
    }
    return nextCursor(cursor, mask);
  }

}  // namespace tinyredis
