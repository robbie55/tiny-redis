#include "core/hash.h"

#include "core/todo.h"

namespace tinyredis {

  std::uint64_t hashBytes(const char* /*p*/, std::size_t /*len*/, std::uint64_t /*seed*/) noexcept {
    unimplemented("hashBytes");
  }

}  // namespace tinyredis
