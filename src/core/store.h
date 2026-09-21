#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "PoolAllocator.h"
#include "core/clock.h"
#include "core/entry.h"

namespace tinyredis {

  // Pool block count. PoolAllocator carves one aligned slab up front and never grows, so
  // this is a hard cap on live keys.
  //
  // TODO(robbie): pick a number and know its cost, since slab bytes = capacity *
  // sizeof(Entry).
  //
  // maxKeys() is capped here, so a failed tryAllocate is a bug, not "full".
#ifndef TINYREDIS_POOL_CAPACITY
#define TINYREDIS_POOL_CAPACITY (1u << 18)
#endif
  inline constexpr std::size_t kPoolCapacity{TINYREDIS_POOL_CAPACITY};

  using EntryPool = PoolAllocator<Entry, kPoolCapacity>;

  // The key/value table.
  //
  // TODO(robbie): open addressing or chaining? Write down why before picking. To narrow
  // it, ask which parts of this codebase hold an Entry* across calls, and what that
  // forbids a rehash from doing.
  //
  // Every method takes `now` from the caller instead of reading the clock.
  // TODO(robbie): what does that buy? Two wins, one in the event loop and one in tests.
  //
  // LRU idle = uint32_t(clock - e.lruClock): unsigned wrap absorbs one ~49.7-day rollover.
  // An expired entry picked by the eviction sampler counts as an expiry, not an eviction.
  //
  // TODO(robbie): declare per tickets 006-008: eviction policy + lruSamples{5} on Config,
  // find(..., touch), evictions(), expiredKeys(), kKeepTtl{-2}, kMaxPasses{16}, cursor.
  class Store {
   public:
    struct Config {
      std::size_t initialBuckets{1024};
      std::size_t maxKeys{0};  // 0 -> bounded only by the pool capacity
      std::uint64_t seed{0};   // 0 -> per-process random seed
    };

    Store();
    explicit Store(const Config& cfg);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;

    // An Entry* from find, and the views from its key()/value(), are valid only until the
    // next call into this Store. Lookups free expired entries, so find and ttlMs count.
    Entry* find(std::string_view key, std::int64_t now);

    // False only under noeviction for a new key when full. Overwrites never need a slot.
    bool set(std::string_view key, std::string_view value, std::int64_t expireAtMs,
             std::int64_t now);

    bool erase(std::string_view key, std::int64_t now);

    // False when the key does not exist.
    bool setExpireAt(std::string_view key, std::int64_t expireAtMs, std::int64_t now);

    // -2 when the key is absent, -1 when it has no TTL, else remaining ms. Redis defines
    // both sentinels, and TTL replies must match.
    std::int64_t ttlMs(std::string_view key, std::int64_t now);

    // Reaps expired entries and returns how many. sampleLimit is soft: a started chain is
    // always finished, or the reverse-binary cursor loses its resize guarantee. The table
    // never shrinks, because shrinking is what makes the cursor revisit keys.
    std::size_t activeExpireCycle(std::int64_t now, std::size_t sampleLimit = 20);

    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t bucketCount() const noexcept;
    [[nodiscard]] std::size_t maxKeys() const noexcept;

   private:
    // TODO(robbie): a suggested split, not a requirement. Add, remove or rename freely.
    Entry* allocEntry(std::string_view key, std::string_view value);
    void freeEntry(Entry* e) noexcept;

    void linkBucket(Entry* e) noexcept;
    void unlinkBucket(Entry* e) noexcept;

    void maybeGrow();
    void rehash(std::size_t newBucketCount);
    [[nodiscard]] std::size_t bucketOf(std::uint64_t h) const noexcept;

    // TODO(robbie): table state. Bucket array, pool, live count, key cap, hash seed, and
    // whatever else the design needs.
  };

}  // namespace tinyredis
