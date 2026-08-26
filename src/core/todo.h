#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace tinyredis {

  // Scaffolding. Every unimplemented stub calls this, so the project always builds and
  // links: you can finish one module, build, and run its tests without the rest of the
  // tree compiling. Any path you have not written yet dies loudly instead of quietly
  // returning a wrong answer.
  //
  // Delete this header when the last TODO is gone.
  [[noreturn]] inline void unimplemented(std::string_view what) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::fprintf(stderr, "unimplemented: %.*s\n", static_cast<int>(what.size()), what.data());
    std::abort();
  }

}  // namespace tinyredis
