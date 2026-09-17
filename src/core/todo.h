#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace tinyredis {

  // Every unwritten stub calls this, so the tree always builds and links and one finished
  // module can be tested without the rest. Unwritten paths abort instead of returning a
  // wrong answer.
  //
  // Delete this header when the last stub is gone.
  [[noreturn]] inline void unimplemented(std::string_view what) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::fprintf(stderr, "unimplemented: %.*s\n", static_cast<int>(what.size()), what.data());
    std::abort();
  }

}  // namespace tinyredis
