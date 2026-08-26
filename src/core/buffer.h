#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace tinyredis {

  // A growable byte buffer with separate read and write cursors. Every connection owns
  // two: one filled by read(2), one drained by write(2).
  //
  // TODO(robbie): two decisions live in here, and both show up in the benchmark.
  //   1. Why a read cursor at all, rather than erasing from the front?
  //   2. ensureWritable() can reclaim the already-consumed prefix or reallocate.
  //      When should it do which?
  class Buffer {
   public:
    static constexpr std::size_t kDefaultCapacity{static_cast<std::size_t>(16 * 1024)};

    explicit Buffer(std::size_t initialCapacity = kDefaultCapacity);

    // The bytes between the read and write cursors.
    [[nodiscard]] std::string_view readable() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

    // Writable tail, so read(2) can land straight in the buffer with no bounce copy.
    char* writePtr() noexcept;
    [[nodiscard]] std::size_t writableBytes() const noexcept;

    // Postcondition: writableBytes() >= n.
    void ensureWritable(std::size_t n);

    // Publishes n bytes written at writePtr().
    void commit(std::size_t n) noexcept;

    // Retires n bytes from the front of readable().
    void consume(std::size_t n) noexcept;

    void append(std::string_view bytes);
    void appendByte(char c);
    void clear() noexcept;

   private:
    // TODO(robbie): your storage and cursors go here.
  };

}  // namespace tinyredis
