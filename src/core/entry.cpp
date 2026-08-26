#include "core/entry.h"

#include "core/todo.h"

namespace tinyredis {

  std::string_view Entry::key() const noexcept { unimplemented("Entry::key"); }

  std::string_view Entry::value() const noexcept { unimplemented("Entry::value"); }

  bool Entry::expired(std::int64_t /*now*/) const noexcept { unimplemented("Entry::expired"); }

}  // namespace tinyredis
