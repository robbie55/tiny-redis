#include "core/store.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <random>

#include "core/clock.h"

namespace {
  constexpr std::uint64_t kRngSalt{0x9e37'79b9'7f4a'7c15ULL};

  std::uint64_t resolveSeed(std::uint64_t seed) {
    if (seed != 0) {
      return seed;
    }
    std::random_device rd;
    return (std::uint64_t{rd()} << 32) | rd();
  }

  // Asserts here, not in the ctor body, so a bad cap fires before table_ sizes to it.
  std::size_t resolveMaxKeys(std::size_t maxKeys) {
    assert(maxKeys <= tinyredis::kPoolCapacity && "Store - maxKeys exceeds pool capacity");
    return maxKeys == 0 ? tinyredis::kPoolCapacity : maxKeys;
  }

  std::uint32_t idle(const tinyredis::Entry* e, const std::int64_t now) {
    return static_cast<std::uint32_t>(now) - e->lruClock;
  }
}  // namespace

namespace tinyredis {

  Store::Store() : Store(Config{}) {}

  Store::Store(const Config& cfg) : Store(cfg, resolveSeed(cfg.seed)) {}

  Store::Store(const Config& cfg, std::uint64_t seed)
      : rngState_{kRngSalt ^ seed},
        maxKeys_{resolveMaxKeys(cfg.maxKeys)},
        expireCursor_{0},
        evictions_{0},
        expiredKeys_{0},
        lruSamples_{cfg.lruSamples},
        // A set cap presizes the table so filling to it never rehashes. An unset one
        // doesn't, or every store would carry a pool-sized bucket array.
        table_{cfg.maxKeys == 0 ? cfg.initialBuckets : std::max(cfg.initialBuckets, cfg.maxKeys),
               seed},
        policy_{cfg.policy} {
    assert(lruSamples_ >= 1 && lruSamples_ <= 64 && "Store - lruSamples outside 1..64");
  }

  Store::~Store() = default;

  Entry* Store::lookup(std::string_view key, std::int64_t now) {
    Entry* found{table_.find(key)};
    if (found == nullptr) {
      return nullptr;
    }

    if (found->expired(now)) {
      table_.remove(found);
      ++expiredKeys_;
      return nullptr;
    }

    return found;
  }

  void Store::evictOne(std::int64_t now) {
    constexpr std::uint32_t kLruSamplesMax{64};

    std::array<Entry*, kLruSamplesMax> buf{};
    std::span<Entry*> out{std::span{buf.data(), lruSamples_}};

    std::size_t n{};

    assert(size() != 0 && "Store::evictOne - function called when table is empty, shouldn't occur");

    while (n == 0) {
      n += table_.sampleSome(out, rngState_);
    }

    bool hasExpiredOne{};
    Entry* toEvict{};

    for (Entry* e : out.first(n)) {
      if (e->expired(now)) {
        table_.remove(e);
        ++expiredKeys_;
        hasExpiredOne = true;
      } else if (toEvict == nullptr || (idle(e, now) > idle(toEvict, now))) {
        toEvict = e;
      }
    }

    if (hasExpiredOne) {
      return;
    }

    table_.remove(toEvict);
    ++evictions_;
  }

  Entry* Store::find(std::string_view key, std::int64_t now, bool touch) {
    Entry* found{lookup(key, now)};
    if (found == nullptr) {
      return nullptr;
    }

    if (touch) {
      found->lruClock = static_cast<std::uint32_t>(now);
    }

    return found;
  }

  bool Store::set(std::string_view key, std::string_view value, std::int64_t expireAtMs,
                  std::int64_t now) {
    Entry* found{lookup(key, now)};

    // past deadline
    if (expireAtMs != kNoExpiry && expireAtMs != kKeepTtl && expireAtMs < now) {
      if (found != nullptr) {
        table_.remove(found);
      }
      return true;
    }

    // found, overwrite
    if (found != nullptr) {
      table_.assign(found, value);
      if (!(kKeepTtl == expireAtMs)) {
        found->expiresAtMs = expireAtMs;
      }
      found->lruClock = static_cast<std::uint32_t>(now);
      return true;
    }

    if (size() >= maxKeys_) {
      switch (policy_) {
        case EvictionPolicy::kNoEviction: {
          return false;
        }
        case EvictionPolicy::kAllKeysLru: {
          evictOne(now);
          break;
        }
      }
    }

    // insert new entry
    Entry* e{table_.insert(key, value)};
    assert(e != nullptr && "Store::set - Inserting in table return nullptr");
    if (expireAtMs == kKeepTtl) {
      e->expiresAtMs = kNoExpiry;
    } else {
      e->expiresAtMs = expireAtMs;
    }

    e->lruClock = static_cast<std::uint32_t>(now);

    return true;
  }

  bool Store::erase(std::string_view key, std::int64_t now) {
    Entry* found{lookup(key, now)};

    if (found == nullptr) {
      return false;
    }

    table_.remove(found);
    return true;
  }

  bool Store::setExpireAt(std::string_view key, std::int64_t expireAtMs, std::int64_t now) {
    Entry* found{lookup(key, now)};

    if (found == nullptr) {
      return false;
    }

    // Because kKeepTtl is -2, it's always less than now, so passing it goes into the past-deadline
    // branch
    assert(expireAtMs != kKeepTtl && "Store::setExpireAt - called with expiresAtMs == kKeepTtl");
    assert(expireAtMs != kNoExpiry && "Store::setExpireAt - called with expiresAtMs == kNoExpiry");

    // past deadline
    if (expireAtMs < now) {
      table_.remove(found);
      return true;
    }

    found->lruClock = static_cast<std::uint32_t>(now);
    found->expiresAtMs = expireAtMs;
    return true;
  }

  std::int64_t Store::ttlMs(std::string_view key, std::int64_t now) {
    Entry* found{lookup(key, now)};
    if (found == nullptr) {
      constexpr std::int64_t kTtlMissing{-2};
      return kTtlMissing;
    }

    if (found->expiresAtMs == kNoExpiry) {
      return kNoExpiry;
    }

    return found->expiresAtMs - now;
  }

  ExpireCycleResult Store::activeExpireCycle(std::int64_t now, std::size_t sampleLimit) {
    std::size_t reaped{};
    std::size_t passes{};
    std::size_t sampled{};
    std::size_t expired{};

    const auto expireVisitor = [&](Entry* e) {
      ++sampled;
      if (e->expired(now)) {
        table_.remove(e);
        ++expired;
      }
    };

    // Redis's empty-bucket cap: bounds a pass's latency on a sparse table.
    const std::size_t maxBuckets{sampleLimit * 20};

    while (passes < kMaxPasses) {
      ++passes;
      sampled = 0;
      expired = 0;

      bool wrapped{false};
      for (std::size_t buckets{}; sampled < sampleLimit && buckets < maxBuckets; ++buckets) {
        expireCursor_ = table_.scanBucket(expireCursor_, expireVisitor);
        if (expireCursor_ == 0) {
          wrapped = true;
          break;
        }
      }

      reaped += expired;
      expiredKeys_ += expired;

      if (wrapped || expired * 10 <= sampled) {
        break;
      }
    }

    return {.reaped = reaped, .passes = passes};
  }

  void Store::clear() {
    table_.clear();
    expireCursor_ = 0;
  }

  std::size_t Store::size() const noexcept { return table_.size(); }

  std::size_t Store::bucketCount() const noexcept { return table_.bucketCount(); }

  std::size_t Store::maxKeys() const noexcept { return maxKeys_; }
  std::size_t Store::evictions() const noexcept { return evictions_; }
  std::size_t Store::expiredKeys() const noexcept { return expiredKeys_; }

}  // namespace tinyredis
