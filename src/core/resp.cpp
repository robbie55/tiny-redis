#include "core/resp.h"

#include "core/todo.h"

namespace tinyredis {

  ParseResult parseCommand(std::string_view /*in*/, Command& /*out*/) noexcept {
    unimplemented("parseCommand");
  }

  namespace reply {

    void simpleString(Buffer& /*out*/, std::string_view /*s*/) {
      unimplemented("reply::simpleString");
    }

    void error(Buffer& /*out*/, std::string_view /*msg*/) { unimplemented("reply::error"); }

    void integer(Buffer& /*out*/, std::int64_t /*v*/) { unimplemented("reply::integer"); }

    void bulk(Buffer& /*out*/, std::string_view /*s*/) { unimplemented("reply::bulk"); }

    void nullBulk(Buffer& /*out*/) { unimplemented("reply::nullBulk"); }

    void arrayHeader(Buffer& /*out*/, std::size_t /*n*/) { unimplemented("reply::arrayHeader"); }

  }  // namespace reply

}  // namespace tinyredis
