#pragma once

#include <chrono>
#include <cstdint>

namespace tinyredis {

  // Milliseconds from a monotonic source. TTLs must not move when the wall clock is
  // stepped by NTP, so steady_clock is the only correct choice here.
  inline std::int64_t nowMs() noexcept {
    const auto t{std::chrono::steady_clock::now().time_since_epoch()};
    return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
  }

  // Sentinel stored in Entry::expireAtMs for a key with no TTL.
  inline constexpr std::int64_t kNoExpiry{-1};

}  // namespace tinyredis
