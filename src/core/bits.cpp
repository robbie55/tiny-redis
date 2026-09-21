#include "core/bits.h"

#include "core/todo.h"

namespace tinyredis {

  std::size_t nextPow2(std::size_t /*n*/) noexcept { unimplemented("nextPow2"); }

  std::size_t nextCursor(std::size_t /*cursor*/, std::size_t /*mask*/) noexcept {
    unimplemented("nextCursor");
  }

  std::uint64_t splitmix64(std::uint64_t& /*state*/) noexcept { unimplemented("splitmix64"); }

}  // namespace tinyredis
