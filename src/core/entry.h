#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tinyredis {

  // One stored key/value pair. Entries come from the pool allocator, so every entry has
  // the same size, fixed at compile time.
  //
  // TODO(robbie): the project's central data-structure decision, and the one you'll most
  // likely be asked about. The layout has to:
  //   - link into a hash chain
  //   - link into an intrusive LRU list, if you take that stretch
  //   - return key() and value() as binary-safe byte ranges, NUL included
  //   - hold an absolute expiry deadline and tell "no TTL" apart from "expires at 0"
  //   - avoid re-reading key bytes when the table rehashes
  //
  // Answer these first:
  //   - redis-benchmark's default key/value pair is ~19 bytes. Should small pairs live
  //     inside the entry or behind a pointer? What does each cost on a GET?
  //   - If small pairs live inline, what happens to a 4 KB value?
  //   - What should sizeof(Entry) be, and how does cache line size bear on it? Pin it
  //     with a static_assert once decided.
  struct Entry {
    // TODO(robbie): your fields go here.

    [[nodiscard]] std::string_view key() const noexcept;
    [[nodiscard]] std::string_view value() const noexcept;

    // True once `now` has reached the entry's deadline. Persistent keys never expire.
    [[nodiscard]] bool expired(std::int64_t now) const noexcept;
  };

}  // namespace tinyredis
