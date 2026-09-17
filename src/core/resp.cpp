#include "core/resp.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include "core/buffer.h"

namespace {
  using namespace tinyredis;

  [[nodiscard]] ParseResult incomplete() noexcept { return ParseResult{}; }

  [[nodiscard]] std::optional<std::size_t> headerValue(std::string_view line, char type,
                                                       std::size_t limit) noexcept {
    if (line.empty() || line.front() != type) {
      return std::nullopt;
    }
    const std::optional<std::size_t> v{detail::readInt(line.substr(1))};
    if (!v.has_value() || *v > limit) {
      return std::nullopt;
    }
    return v;
  }

  // Sets argc and blanks the slots past it, so a successful parse needs no clear on entry.
  void publish(Command& out, std::size_t argc) noexcept {
    out.argc = argc;
    std::fill_n(out.argv.begin() + static_cast<std::ptrdiff_t>(argc), kMaxArgs - argc,
                std::string_view{});
  }

  void appendLine(Buffer& out, char type, std::string_view body) {
    out.appendByte(type);
    out.append(body);
    out.append("\r\n");
  }

  // Formats on the stack so no reply allocates. 20 digits and a sign cover any 64-bit value.
  template <std::integral T>
  void appendNumber(Buffer& out, char type, T v) {
    std::array<char, 24> digits{};
    const auto [end, ec]{std::to_chars(digits.data(), digits.data() + digits.size(), v)};
    assert(ec == std::errc{});
    appendLine(out, type,
               std::string_view{digits.data(), static_cast<std::size_t>(end - digits.data())});
  }

}  // namespace

namespace tinyredis::detail {
  void clear(Command& cmd) {
    cmd.argc = 0;
    std::ranges::fill(cmd.argv, std::string_view{});
  }

  ParseResult parseInline(std::string_view in, Command& out) noexcept {
    const Line line{readLine(in, 0, kMaxInlineLength, Framing::kInline)};
    if (line.status == LineStatus::kIncomplete) {
      return incomplete();
    }
    if (line.status == LineStatus::kTooLong) {
      return fail(Error::kInlineLength);
    }

    const std::string_view text{line.text};
    std::size_t n{};
    std::size_t pos{};

    while (true) {
      pos = text.find_first_not_of(" \t", pos);
      if (pos == std::string_view::npos) {
        break;
      }

      const auto start{pos};
      pos = text.find_first_of(" \t", start);

      if (n == kMaxArgs) {
        return fail(Error::kArgLength);
      }

      out.argv[n++] = text.substr(start, pos - start);
    }

    publish(out, n);

    return ParseResult{.status = ParseStatus::kOk, .consumed = line.next, .error = {}};
  }

  ParseResult parseArray(std::string_view in, Command& out) noexcept {
    const Line header{readLine(in, 0, kMaxHeaderLength, Framing::kStrict)};
    if (header.status == LineStatus::kIncomplete) {
      return incomplete();
    }
    if (header.status == LineStatus::kTooLong) {
      return fail(Error::kArgLength);
    }

    // Bounds the unchecked argv[i] below, so it must run before any element is parsed.
    const std::optional<std::size_t> count{headerValue(header.text, '*', kMaxArgs)};
    if (!count.has_value()) {
      return fail(Error::kArgLength);
    }

    std::size_t cursor{header.next};

    for (std::size_t i{}; i < *count; ++i) {
      const Line elem{readLine(in, cursor, kMaxHeaderLength, Framing::kStrict)};
      if (elem.status == LineStatus::kIncomplete) {
        return incomplete();
      }
      if (elem.status == LineStatus::kTooLong) {
        return fail(Error::kBulkLength);
      }

      const std::optional<std::size_t> len{headerValue(elem.text, '$', kMaxBulkLength)};
      if (!len.has_value()) {
        return fail(Error::kBulkLength);
      }

      const Line payload{takeBytes(in, elem.next, *len)};
      if (payload.status == LineStatus::kIncomplete) {
        return incomplete();
      }
      if (payload.status == LineStatus::kBadTrailer) {
        return fail(Error::kBulkLength);
      }

      // argc stays 0 until publish(), so a half-framed command never looks complete.
      out.argv[i] = payload.text;
      cursor = payload.next;
    }

    publish(out, *count);
    return ParseResult{.status = ParseStatus::kOk, .consumed = cursor, .error = {}};
  }

  ParseResult fail(Error e) noexcept {
    ParseResult res{};
    res.status = ParseStatus::kProtocolError;
    res.error = errorToString(e);
    return res;
  }

  Line takeBytes(std::string_view in, std::size_t at, std::size_t len) noexcept {
    Line res{};  // kIncomplete

    if (at > in.size()) {
      return res;
    }

    // Subtract rather than add. `at + len + 2` can wrap, since readInt accepts values near
    // SIZE_MAX.
    const std::size_t avail{in.size() - at};
    if (avail < 2 || len > avail - 2) {
      return res;
    }

    // Redis skips the trailer unchecked. A bad one means framing has desynced, so fail.
    if (in[at + len] != '\r' || in[at + len + 1] != '\n') {
      res.status = LineStatus::kBadTrailer;
      return res;
    }

    res.status = LineStatus::kFound;
    res.text = in.substr(at, len);
    res.next = at + len + 2;
    return res;
  }

  Line readLine(std::string_view in, std::size_t at, std::size_t maxLen, Framing framing) noexcept {
    Line res{};  // kIncomplete

    if (at >= in.size()) {
      return res;
    }

    const std::size_t avail{in.size() - at};

    // A legal line's LF sits at index maxLen + 1 at the furthest: maxLen bytes of text then
    // CRLF. Nothing past that window can still be a line worth keeping.
    const std::size_t window{std::min(avail, maxLen + 2)};
    const std::string_view scan{in.substr(at, window)};

    // LF is the only terminator; CRLF is an LF with a CR in front. Searching for the CR
    // instead would let a later CRLF win over an earlier bare LF.
    std::size_t lf{};
    bool cr{};
    for (std::size_t from{};; from = lf + 1) {
      lf = scan.find('\n', from);
      if (lf == std::string_view::npos) {
        // Only with the whole window in hand can we rule out a legal line still arriving.
        if (avail >= maxLen + 2 || (avail == maxLen + 1 && scan.back() != '\r')) {
          res.status = LineStatus::kTooLong;
        }
        return res;
      }

      // `at` always starts a line, so a CR before it belongs to the previous terminator and
      // looking back no further than the window is correct.
      cr = lf > 0 && scan[lf - 1] == '\r';
      if (cr || framing == Framing::kInline) {
        break;
      }
    }

    // Same index, different verdicts: a CRLF ending at maxLen + 1 is a legal maxLen-byte
    // line, a bare LF there is one byte too many. Only the text length can tell them apart.
    const std::size_t len{cr ? lf - 1 : lf};
    if (len > maxLen) {
      res.status = LineStatus::kTooLong;
      return res;
    }

    res.status = LineStatus::kFound;
    res.text = in.substr(at, len);
    res.next = at + lf + 1;
    return res;
  }

  std::optional<std::size_t> readInt(std::string_view s) noexcept {
    if (s.empty()) {
      return std::nullopt;
    }

    // Zero has one spelling. Handling it first lets the lead-digit check below reject
    // "007", "+1" and "-1" in one comparison.
    if (s == "0") {
      return 0;
    }
    if (s.front() < '1' || s.front() > '9') {
      return std::nullopt;
    }

    // Exact bound: v * 10 + d <= max  iff  v <= (max - d) / 10.
    constexpr std::size_t kLimit{std::numeric_limits<std::size_t>::max()};

    std::size_t v{0};
    for (const char c : s) {
      if (c < '0' || c > '9') {
        return std::nullopt;
      }
      const auto d{static_cast<std::size_t>(c - '0')};
      if (v > (kLimit - d) / 10U) {
        return std::nullopt;
      }
      v = (v * 10U) + d;
    }

    return v;
  }

}  // namespace tinyredis::detail

namespace tinyredis {
  ParseResult parseCommand(std::string_view in, Command& out) noexcept {
    // No clear on entry. On success publish() blanks only the slots past argc.
    if (in.empty()) {
      detail::clear(out);
      return incomplete();
    }

    ParseResult res{};
    if (in.front() == '*') {
      res = detail::parseArray(in, out);
    } else {
      res = detail::parseInline(in, out);
    }

    // Sub-parsers can fail after writing argv slots. This is the one place that clears them.
    if (res.status != ParseStatus::kOk) {
      detail::clear(out);
    }

    return res;
  }

  namespace reply {
    void simpleString(Buffer& out, std::string_view s) {
      assert(s.find(" \t"));
      appendLine(out, '+', s);
    }

    void error(Buffer& out, std::string_view msg) { appendLine(out, '-', msg); }

    void integer(Buffer& out, std::int64_t v) { appendNumber(out, ':', v); }

    void bulk(Buffer& out, std::string_view s) {
      appendNumber(out, '$', s.size());
      out.append(s);
      out.append("\r\n");
    }

    void nullBulk(Buffer& out) { out.append("$-1\r\n"); }

    void arrayHeader(Buffer& out, std::size_t n) { appendNumber(out, '*', n); }

  }  // namespace reply

}  // namespace tinyredis
