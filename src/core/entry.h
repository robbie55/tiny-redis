#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tinyredis {

  // One stored key/value pair. Entries are carved from the pool allocator, so every
  // entry is the same size and that size is fixed at compile time.
  //
  // TODO(robbie): this is the central data-structure decision in the project, and the one
  // you are most likely to be asked about. Design the layout yourself. It has to let you:
  //
  //   - link the entry into a hash chain
  //   - link it into an intrusive LRU list, if you take that stretch
  //   - answer key() and value() as byte ranges, binary-safe (NUL is a legal byte)
  //   - carry an absolute expiry deadline, and distinguish "no TTL" from "expires at 0"
  //   - avoid re-reading key bytes when the table rehashes
  //
  // Questions worth answering before you write it:
  //   - redis-benchmark's default key/value pair is ~19 bytes. Should small pairs live
  //     inside the entry, or behind a pointer? What does each cost on a GET?
  //   - If small pairs live inline, what happens to a 4 KB value?
  //   - What should sizeof(Entry) be, and what does the cache line size have to do with
  //     it? Consider a static_assert once you have decided, so it cannot drift.
  struct Entry {
    // TODO(robbie): your fields go here.

    [[nodiscard]] std::string_view key() const noexcept;
    [[nodiscard]] std::string_view value() const noexcept;

    // True once `now` has reached the entry's deadline. Persistent keys never expire.
    [[nodiscard]] bool expired(std::int64_t now) const noexcept;
  };

}  // namespace tinyredis
