#include "core/buffer.h"

#include "core/todo.h"

namespace tinyredis {

  Buffer::Buffer(std::size_t /*initialCapacity*/) { unimplemented("Buffer::Buffer"); }

  std::string_view Buffer::readable() const noexcept { unimplemented("Buffer::readable"); }

  std::size_t Buffer::size() const noexcept { unimplemented("Buffer::size"); }

  bool Buffer::empty() const noexcept { unimplemented("Buffer::empty"); }

  std::size_t Buffer::capacity() const noexcept { unimplemented("Buffer::capacity"); }

  char* Buffer::writePtr() noexcept { unimplemented("Buffer::writePtr"); }

  std::size_t Buffer::writableBytes() const noexcept { unimplemented("Buffer::writableBytes"); }

  // TODO(robbie): compact before you grow, and think about what "enough room" means once
  // a pipelined batch has left a consumed prefix behind.
  void Buffer::ensureWritable(std::size_t /*n*/) { unimplemented("Buffer::ensureWritable"); }

  void Buffer::commit(std::size_t /*n*/) noexcept { unimplemented("Buffer::commit"); }

  void Buffer::consume(std::size_t /*n*/) noexcept { unimplemented("Buffer::consume"); }

  void Buffer::append(std::string_view /*bytes*/) { unimplemented("Buffer::append"); }

  void Buffer::appendByte(char /*c*/) { unimplemented("Buffer::appendByte"); }

  void Buffer::clear() noexcept { unimplemented("Buffer::clear"); }

}  // namespace tinyredis
