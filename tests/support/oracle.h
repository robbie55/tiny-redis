#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

#include "core/clock.h"
#include "core/store.h"
#include "support/kv.h"

namespace tinyredis {

  struct OracleResult {
    std::vector<double> percentiles;  // victim rank / live keys; 0 = LRU (ideal), ->1 = MRU
    std::size_t bad;                  // victims in the most-recent 20%
  };

  // True-LRU oracle beside an allkeys-lru Store (ticket 009). Fills s to maxKeys(), then
  // draws ids from gen until warmup + evictions evictions, recording only the last
  // `evictions`. Victim lookup walks oldest-first until the first absent key: O(rank) per
  // eviction. That is sound only because nothing else removes keys, so no TTLs.
  // Aborts if Store and oracle disagree: a harness or Store bug, not a measurement.
  OracleResult runOracle(Store& s, auto& gen, std::size_t universe, std::size_t warmup,
                         std::size_t evictions) {
    constexpr std::int64_t kAbsent{-1};
    std::vector<std::int64_t> lastUse(universe, kAbsent);
    std::set<std::pair<std::int64_t, std::size_t>> byRecency;  // oldest first

    // Every op gets its own tick, so neither side ever sees a recency tie.
    std::int64_t now{1'000};
    const auto use{[&](std::size_t id) {
      if (lastUse[id] != kAbsent) {
        byRecency.erase({lastUse[id], id});
      }
      lastUse[id] = now;
      byRecency.insert({now, id});
    }};
    const auto fail{[](const char* why) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      std::fprintf(stderr, "runOracle: %s\n", why);
      std::abort();
    }};

    for (std::size_t id{0}; id < s.maxKeys(); ++id, ++now) {
      if (!s.set(key(id), "v", kNoExpiry, now)) {
        fail("fill set failed");
      }
      use(id);
    }

    OracleResult r{.percentiles = {}, .bad = 0};
    r.percentiles.reserve(evictions);
    for (std::size_t evicted{0}; evicted < warmup + evictions;) {
      ++now;
      const std::size_t id{gen.next()};
      if (s.find(key(id), now) != nullptr) {
        use(id);
        continue;
      }
      if (!s.set(key(id), "v", kNoExpiry, now)) {
        fail("set failed; policy not allkeys-lru?");
      }

      // touch=false: a touching probe would reorder the Store's LRU mid-measurement.
      std::size_t rank{0};
      auto it{byRecency.begin()};
      for (; it != byRecency.end() && s.find(key(it->second), now, false) != nullptr; ++it) {
        ++rank;
      }
      if (it == byRecency.end()) {
        fail("no victim found");
      }

      if (evicted >= warmup) {
        const std::size_t n{byRecency.size()};
        r.percentiles.push_back(static_cast<double>(rank) / static_cast<double>(n));
        if (rank * 5 >= n * 4) {
          ++r.bad;
        }
      }
      lastUse[it->second] = kAbsent;
      byRecency.erase(it);
      use(id);
      ++evicted;
    }
    return r;
  }

}  // namespace tinyredis
