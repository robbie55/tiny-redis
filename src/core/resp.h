#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/buffer.h"

// RESP2 request parsing and reply encoding.
// Spec: https://redis.io/docs/latest/develop/reference/protocol-spec/

namespace tinyredis {
  enum class Error : uint8_t { kArgLength, kBulkLength, kInlineLength };

  constexpr std::string_view errorToString(Error e) {
    switch (e) {
      case Error::kArgLength:
        return "ERR Protocol error: invalid multibulk length";
      case Error::kBulkLength:
        return "ERR Protocol error: invalid bulk length";
      case Error::kInlineLength:
        return "ERR Protocol error: too big inline request";
      default:
        return "ERR Unknown";
    }
  }

  // SET key value EX n is the widest command we accept, at five args.
  inline constexpr std::size_t kMaxArgs{16};

  // Redis defaults proto-max-bulk-len to 512 MB. This cap is tighter.
  inline constexpr std::size_t kMaxBulkLength{static_cast<std::size_t>(64 * 1024 * 1024)};

  // Redis's PROTO_INLINE_MAX_SIZE. Inline has no length prefix, so this is the only bound
  // on an unterminated line.
  inline constexpr std::size_t kMaxInlineLength{static_cast<std::size_t>(64 * 1024)};

  // One parsed command. argv views point into the caller's input buffer.
  struct Command {
    std::array<std::string_view, kMaxArgs> argv{};
    std::size_t argc{0};

    std::string_view operator[](std::size_t i) const noexcept { return argv[i]; }  // NOLINT
  };

  enum class ParseStatus : std::uint8_t { kOk, kIncomplete, kProtocolError };

  struct ParseResult {
    ParseStatus status{ParseStatus::kIncomplete};
    std::size_t consumed{};
    std::string_view error;
  };

  // parseCommand internals. Public for tests only.
  namespace detail {
    // Room for a type byte, a sign and any 64-bit count. An overlong count still reads as a
    // line and fails as a bad length.
    inline constexpr std::size_t kMaxHeaderLength{32};

    enum class LineStatus : std::uint8_t {
      kFound,
      kIncomplete,  // no terminator yet, still within budget
      kTooLong,     // no legal line fits in maxLen, the caller picks the error
      kBadTrailer,  // takeBytes only, payload arrived without CRLF after it
    };

    struct Line {
      LineStatus status{LineStatus::kIncomplete};
      std::string_view text;  // terminator excluded
      std::size_t next{};     // offset past the terminator, kFound only
    };

    // Inline commands may end at a bare LF; the array form requires CRLF.
    enum class Framing : std::uint8_t {
      kStrict,
      kInline,
    };

    Line readLine(std::string_view in, std::size_t at, std::size_t maxLen,
                  Framing framing) noexcept;

    // Takes exactly `len` bytes at `at` and checks that CRLF follows. Never scans the
    // payload, since NUL, CR and LF are all legal inside it.
    [[nodiscard]] Line takeBytes(std::string_view in, std::size_t at, std::size_t len) noexcept;

    // Parses header digits with the type byte and CRLF already stripped. Canonical unsigned
    // decimal only, so "0" passes and "007", "+1", "-1", "12 " don't. nullopt on overflow.
    // Redis treats "*-1" as a no-op. Here it's a protocol error.
    [[nodiscard]] std::optional<std::size_t> readInt(std::string_view s) noexcept;

    // Protocol-error result with consumed = 0. The connection closes after replying.
    [[nodiscard]] ParseResult fail(Error e) noexcept;

    void clear(Command& cmd);

    ParseResult parseArray(std::string_view in, Command& out) noexcept;
    ParseResult parseInline(std::string_view in, Command& out) noexcept;

  }  // namespace detail

  // Parses one command, array or inline form, from the front of `in`. A strict prefix of a
  // valid command returns kIncomplete and consumes nothing.
  //
  // `out` is valid only on kOk, with every slot past argc empty. Any other status clears it.
  // Callers reuse one Command across a pipelined batch, so a stale slot would hand an arity
  // bug the previous command's bytes.
  ParseResult parseCommand(std::string_view in, Command& out) noexcept;

  // Reply encoders. Each appends one RESP value to `out`.
  namespace reply {

    void simpleString(Buffer& out, std::string_view s);
    void error(Buffer& out, std::string_view msg);
    void integer(Buffer& out, std::int64_t v);
    void bulk(Buffer& out, std::string_view s);
    void nullBulk(Buffer& out);
    void arrayHeader(Buffer& out, std::size_t n);

  }  // namespace reply

}  // namespace tinyredis
