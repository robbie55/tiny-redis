#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "core/clock.h"

namespace tinyredis {

  // One stored key/value pair. Entries come from the pool allocator, so every entry has
  // the same size, fixed at compile time.
  inline constexpr std::uint32_t kInlineCap{32};

  // redis-benchmark's default pair: key "key:__rand_int__" (16 B, -r keeps it 16 by
  // zero-padding), value of -d bytes, default 3.
  inline constexpr std::uint32_t kBenchKeyLen{16};
  inline constexpr std::uint32_t kBenchValLen{3};

  // Any padding inside Entry, including the tail, is a build error. The sizeof assert can't
  // see a hole that a reorder opens while the total stays at 64; this can. __GNUC__ is
  // defined by both GCC and Clang, and Clang honours the "GCC" pragma spelling.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpadded"
#endif
  struct alignas(64) Entry {
   private:
    [[nodiscard]] std::string_view getBufData(uint32_t start, uint32_t size) const noexcept {
      if (spilled()) {
        return {bufPtr + start, size};
      }

      return {inlineBuf.data() + start, size};
    }

   public:
    Entry* next;
    std::int64_t expiresAtMs;

    std::uint32_t hash;
    std::uint32_t lruClock;
    std::uint32_t keyLen;
    std::uint32_t valLen;

    union {
      std::array<char, kInlineCap> inlineBuf;
      char* bufPtr;
    };

    [[nodiscard]] std::string_view key() const noexcept;

    [[nodiscard]] bool spilled() const noexcept { return keyLen + valLen > kInlineCap; }

    [[nodiscard]] std::string_view value() const noexcept;

    // True once `now` has exceeded the entry's deadline. Persistent keys never expire.
    [[nodiscard]] bool expired(std::int64_t now) const noexcept;
  };
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  static_assert(sizeof(Entry) == 64, "tinyredis::Entry - Size isn't 64b");
  static_assert(alignof(Entry) == 64, "tinyredis::Entry - Alignment isn't 64b");
  static_assert(kInlineCap >= kBenchKeyLen + kBenchValLen,
                "tinyredis::Entry - Inline cap can't cover bench max key + val size");

}  // namespace tinyredis
