#include "core/command.h"

#include "core/todo.h"

namespace tinyredis {

  DispatchResult dispatch(Store& /*store*/, const Command& /*cmd*/, std::int64_t /*now*/,
                          Buffer& /*out*/) {
    unimplemented("dispatch");
  }

}  // namespace tinyredis
