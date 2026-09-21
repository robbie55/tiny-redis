#include "core/entry.h"

namespace tinyredis {

  std::string_view Entry::key() const noexcept { return getBufData(0, keyLen); }

  std::string_view Entry::value() const noexcept { return getBufData(keyLen, valLen); }

  bool Entry::expired(std::int64_t now) const noexcept {
    if (expiresAtMs == kNoExpiry) {
      return false;
    }
    return now > expiresAtMs;
  }

}  // namespace tinyredis
