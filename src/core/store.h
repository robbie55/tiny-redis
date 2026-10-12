#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/entry.h"
#include "core/table.h"

namespace tinyredis {

  // Key/value policy over a Table: lazy expiry, eviction, the expire cycle, counters.
  // LRU idle = uint32_t(clock - e.lruClock): unsigned wrap absorbs one ~49.7-day rollover.
  // An expired entry picked by the eviction sampler counts as an expiry, not an eviction.
  inline constexpr std::size_t kMaxPasses{16};
  inline constexpr std::int64_t kKeepTtl{-2};

  enum class EvictionPolicy : std::uint8_t { kNoEviction, kAllKeysLru };

  struct ExpireCycleResult {
    std::size_t reaped;
    std::size_t passes;
  };

  class Store {
   public:
    struct Config {
      std::size_t initialBuckets{1024};                    // rounded up to a power of two
      std::size_t maxKeys{0};                              // 0 -> bounded only by the pool capacity
      std::uint64_t seed{0};                               // 0 -> per-process random seed
      std::uint32_t lruSamples{5};                         // 1...64, max-mem samples
      EvictionPolicy policy{EvictionPolicy::kNoEviction};  // redis default
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
    // touch=false for reads that aren't use (EXISTS)
    Entry* find(std::string_view key, std::int64_t now, bool touch = true);

    // False only under noeviction for a new key when full. Overwrites never need a slot.
    bool set(std::string_view key, std::string_view value, std::int64_t expireAtMs,
             std::int64_t now);

    bool erase(std::string_view key, std::int64_t now);

    // False when the key does not exist.
    bool setExpireAt(std::string_view key, std::int64_t expireAtMs, std::int64_t now);

    // -2 when the key is absent, -1 when it has no TTL, else remaining ms. Redis defines
    // both sentinels, and TTL replies must match.
    std::int64_t ttlMs(std::string_view key, std::int64_t now);

    // sampleLimit is soft: a started chain is always finished, or the reverse-binary
    // cursor loses its resize guarantee. passes is capped at kMaxPasses.
    ExpireCycleResult activeExpireCycle(std::int64_t now, std::size_t sampleLimit = 20);

    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t bucketCount() const noexcept;
    [[nodiscard]] std::size_t maxKeys() const noexcept;
    [[nodiscard]] std::size_t evictions() const noexcept;
    [[nodiscard]] std::size_t expiredKeys() const noexcept;

   private:
    Store(const Config& cfg, std::uint64_t seed);

    Entry* lookup(std::string_view key, std::int64_t now);
    void evictOne(std::int64_t now);

    std::uint64_t rngState_;  // splitmix sample for Table::sampleSome
    std::size_t maxKeys_;
    std::size_t expireCursor_;  // reverse binary cursor
    std::size_t evictions_;     // counter
    std::size_t expiredKeys_;   // counter
    std::uint32_t lruSamples_;  // from Config

    Table table_;
    EvictionPolicy policy_;
  };

}  // namespace tinyredis
