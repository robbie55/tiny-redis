#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/buffer.h"

namespace tinyredis {

  // TODO(robbie): these are limits, not capacities. A parser that trusts a length prefix
  // off the wire will happily try to allocate whatever a client claims. Decide what each
  // bound should be and what you reply when one is exceeded.
  inline constexpr std::size_t kMaxArgs{16};
  inline constexpr std::size_t kMaxBulkLength{static_cast<std::size_t>(64 * 1024 * 1024)};
  inline constexpr std::size_t kMaxInlineLength{static_cast<std::size_t>(64 * 1024)};

  // One parsed command.
  //
  // TODO(robbie): the arguments are string_views, not strings. Into what do they point,
  // and what does that forbid the caller from doing before the reply has been written?
  // This is the "zero-copy parsing" claim in the README -- make sure you can defend it.
  struct Command {
    std::array<std::string_view, kMaxArgs> argv{};
    std::size_t argc{0};

    std::string_view operator[](std::size_t i) const noexcept { return argv[i]; }  // NOLINT
  };

  enum class ParseStatus : std::uint8_t {
    kOk,            // a complete command was produced
    kIncomplete,    // need more bytes; call again after the next read
    kProtocolError  // unrecoverable
  };

  struct ParseResult {
    ParseStatus status{ParseStatus::kIncomplete};
    std::size_t consumed{};
    std::string_view error;
  };

  // Parses ONE command from the front of `in`.
  //
  // TODO(robbie): two wire forms have to work.
  //   - RESP arrays of bulk strings: "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"
  //     This is what redis-cli and redis-benchmark send. Get this one right first.
  //   - Inline commands: "PING\r\n" typed straight at a socket.
  //
  // The hard part is not the happy path, it is that TCP hands you arbitrary fragments.
  // Every prefix of a valid command must return kIncomplete and consume nothing, and the
  // next call after more bytes arrive must succeed. Test it byte-by-byte.
  ParseResult parseCommand(std::string_view in, Command& out) noexcept;

  // Reply encoders. Each appends one RESP value to `out`.
  // TODO(robbie): spec at https://redis.io/docs/latest/develop/reference/protocol-spec/
  // Note the difference between a null bulk string and an empty one -- GET on a missing
  // key and GET on a key set to "" must not produce the same bytes.
  namespace reply {

    void simpleString(Buffer& out, std::string_view s);
    void error(Buffer& out, std::string_view msg);
    void integer(Buffer& out, std::int64_t v);
    void bulk(Buffer& out, std::string_view s);
    void nullBulk(Buffer& out);
    void arrayHeader(Buffer& out, std::size_t n);

  }  // namespace reply

}  // namespace tinyredis
