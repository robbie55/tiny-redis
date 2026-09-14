#include "core/buffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace tinyredis {

  Buffer::Buffer(std::size_t initialCapacity) { buf_.resize(initialCapacity); }

  std::string_view Buffer::readable() const noexcept {
    return std::string_view{buf_.data() + readIdx_, size()};
  }

  std::size_t Buffer::size() const noexcept { return writeIdx_ - readIdx_; }

  bool Buffer::empty() const noexcept { return readIdx_ == writeIdx_; }

  std::size_t Buffer::capacity() const noexcept { return buf_.size(); }

  char* Buffer::writePtr() noexcept { return buf_.data() + writeIdx_; }

  std::size_t Buffer::writableBytes() const noexcept { return buf_.size() - writeIdx_; }

  void Buffer::ensureWritable(std::size_t n) {
    // enough room at the tail already
    if (writableBytes() >= n) {
      return;
    }

    // compact even if we end up resizing
    compact();

    // compaction reclaimed the consumed prefix
    if (writableBytes() >= n) {
      return;
    }

    buf_.resize(std::max(2 * capacity(), size() + n));
  }

  void Buffer::commit(std::size_t n) noexcept {
    assert(n <= writableBytes() && "Overflow of size_t write cursor in Buffer");
    writeIdx_ += n;
  }

  void Buffer::consume(std::size_t n) noexcept {
    assert(n <= size() && "read cursor passed write cursor in Buffer");
    readIdx_ += n;
    if (empty()) {
      readIdx_ = 0;
      writeIdx_ = 0;
    }
  }

  void Buffer::append(std::string_view bytes) {
    if (bytes.empty()) {
      return;
    }

    const auto n{bytes.size()};

    ensureWritable(n);
    std::memcpy(writePtr(), bytes.data(), n);
    commit(n);
  }

  void Buffer::appendByte(char c) {
    ensureWritable(1);
    buf_[writeIdx_] = c;
    commit(1);
  }

  void Buffer::clear() noexcept {
    readIdx_ = 0;
    writeIdx_ = 0;
  }

  void Buffer::compact() noexcept {
    const auto curSize{size()};

    std::memmove(buf_.data(), buf_.data() + readIdx_, curSize);

    writeIdx_ = curSize;
    readIdx_ = 0;
  }

}  // namespace tinyredis
