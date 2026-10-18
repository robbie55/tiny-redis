#pragma once

#include <cstdint>

#include "core/buffer.h"
#include "core/resp.h"
#include "core/store.h"

namespace tinyredis {

  enum class DispatchResult : std::uint8_t { kContinue, kCloseConnection };

  // Returns kCloseConnection for QUIT and for anything the connection can't recover from.
  DispatchResult dispatch(Store& store, const Command& cmd, std::int64_t now, Buffer& out);

}  // namespace tinyredis
