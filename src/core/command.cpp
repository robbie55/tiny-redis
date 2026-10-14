#include "core/command.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "core/buffer.h"
#include "core/clock.h"
#include "core/entry.h"
#include "core/resp.h"
#include "core/store.h"

namespace tinyredis {
  namespace {

    enum class Cmd : std::uint8_t {
      kGet,
      kSet,
      kDel,
      kExists,
      kExpire,
      kTtl,
      kPing,
      kEcho,
      kDbsize,
      kFlushall,
      kQuit,
    };

    // Redis arity convention: counts the name; > 0 is exact, < 0 is a minimum.
    struct Spec {
      std::string_view name;  // lowercase, also the spelling used in error replies
      Cmd cmd;
      int arity;
    };

    constexpr std::array kSpecs{
        Spec{.name = "get", .cmd = Cmd::kGet, .arity = 2},
        Spec{.name = "set", .cmd = Cmd::kSet, .arity = -3},
        Spec{.name = "del", .cmd = Cmd::kDel, .arity = -2},
        Spec{.name = "exists", .cmd = Cmd::kExists, .arity = -2},
        Spec{.name = "expire", .cmd = Cmd::kExpire, .arity = 3},
        Spec{.name = "ttl", .cmd = Cmd::kTtl, .arity = 2},
        Spec{.name = "ping", .cmd = Cmd::kPing, .arity = -1},
        Spec{.name = "echo", .cmd = Cmd::kEcho, .arity = 2},
        Spec{.name = "dbsize", .cmd = Cmd::kDbsize, .arity = 1},
        Spec{.name = "flushall", .cmd = Cmd::kFlushall, .arity = -1},
        Spec{.name = "quit", .cmd = Cmd::kQuit, .arity = -1},
    };

    constexpr std::int64_t kMsPerSec{1000};

    constexpr std::string_view kNotInteger{"ERR value is not an integer or out of range"};
    constexpr std::string_view kSyntax{"ERR syntax error"};
    constexpr std::string_view kOom{"OOM command not allowed when used memory > 'maxmemory'."};

    // ASCII only: Redis command names and options are ASCII, and locale-aware tolower would
    // fold bytes differently per process.
    constexpr char lower(char c) noexcept {
      return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    // `lowerRef` must already be lowercase.
    constexpr bool iequals(std::string_view s, std::string_view lowerRef) noexcept {
      if (s.size() != lowerRef.size()) {
        return false;
      }
      for (std::size_t i{0}; i < s.size(); ++i) {
        if (lower(s[i]) != lowerRef[i]) {
          return false;
        }
      }
      return true;
    }

    const Spec* lookup(std::string_view name) noexcept {
      for (const Spec& s : kSpecs) {
        if (iequals(name, s.name)) {
          return &s;
        }
      }
      return nullptr;
    }

    bool arityOk(const Spec& spec, std::size_t argc) noexcept {
      const auto n{static_cast<std::int64_t>(argc)};
      return spec.arity > 0 ? n == spec.arity : n >= -spec.arity;
    }

    // Matches Redis's string2ll closely enough for the wire: no sign prefix, no whitespace,
    // whole string consumed.
    std::optional<std::int64_t> parseInt(std::string_view s) noexcept {
      std::int64_t v{};
      const auto [ptr, ec]{std::from_chars(s.data(), s.data() + s.size(), v)};
      if (ec != std::errc{} || ptr != s.data() + s.size() || s.empty()) {
        return std::nullopt;
      }
      return v;
    }

    // `n` > 0. nullopt when the absolute deadline would overflow.
    std::optional<std::int64_t> deadline(std::int64_t n, std::int64_t unitMs,
                                         std::int64_t now) noexcept {
      if (n > (std::numeric_limits<std::int64_t>::max() - now) / unitMs) {
        return std::nullopt;
      }
      return now + (n * unitMs);
    }

    // Error replies are simple strings, so a client-supplied CR or LF echoed into one would
    // split the frame and desync every reply after it. Redis does the same scrub.
    void appendSanitized(std::string& dst, std::string_view s) {
      constexpr std::size_t kMaxEcho{128};
      for (const char c : s.substr(0, kMaxEcho)) {
        dst.push_back(c == '\r' || c == '\n' ? ' ' : c);
      }
    }

    // Cold path; allocating here is fine.
    void unknownCommand(const Command& cmd, Buffer& out) {
      std::string msg{"ERR unknown command '"};
      appendSanitized(msg, cmd[0]);
      msg += "', with args beginning with: ";
      for (std::size_t i{1}; i < cmd.argc; ++i) {
        msg += '\'';
        appendSanitized(msg, cmd[i]);
        msg += "' ";
      }
      reply::error(out, msg);
    }

    void wrongArity(std::string_view name, Buffer& out) {
      std::string msg{"ERR wrong number of arguments for '"};
      msg += name;
      msg += "' command";
      reply::error(out, msg);
    }

    void invalidExpire(std::string_view name, Buffer& out) {
      std::string msg{"ERR invalid expire time in '"};
      msg += name;
      msg += "' command";
      reply::error(out, msg);
    }

    // SET key value [EX s | PX ms | KEEPTTL]. A bare SET clears any TTL (kNoExpiry);
    // KEEPTTL is the opt-out. NX/XX/GET are not supported and read as syntax errors.
    void cmdSet(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      std::int64_t expireAt{kNoExpiry};
      bool haveExpiryOpt{false};

      for (std::size_t i{3}; i < cmd.argc; ++i) {
        const std::string_view opt{cmd[i]};
        if (iequals(opt, "keepttl") && !haveExpiryOpt) {
          expireAt = kKeepTtl;
          haveExpiryOpt = true;
          continue;
        }

        const bool ex{iequals(opt, "ex")};
        if (!(ex || iequals(opt, "px")) || haveExpiryOpt || i + 1 >= cmd.argc) {
          reply::error(out, kSyntax);
          return;
        }

        const auto n{parseInt(cmd[++i])};
        if (!n) {
          reply::error(out, kNotInteger);
          return;
        }
        const auto at{*n > 0 ? deadline(*n, ex ? kMsPerSec : 1, now) : std::nullopt};
        if (!at) {
          invalidExpire("set", out);
          return;
        }
        expireAt = *at;
        haveExpiryOpt = true;
      }

      if (!store.set(cmd[1], cmd[2], expireAt, now)) {
        reply::error(out, kOom);
        return;
      }
      reply::simpleString(out, "OK");
    }

    // EXPIRE key seconds. Non-positive deletes the key now, as Redis does; Store's own
    // past-deadline rule (deadline < now) would let `EXPIRE k 0` live for one more ms.
    void cmdExpire(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      const auto secs{parseInt(cmd[2])};
      if (!secs) {
        reply::error(out, kNotInteger);
        return;
      }
      if (*secs <= 0) {
        reply::integer(out, store.erase(cmd[1], now) ? 1 : 0);
        return;
      }
      const auto at{deadline(*secs, kMsPerSec, now)};
      if (!at) {
        invalidExpire("expire", out);
        return;
      }
      reply::integer(out, store.setExpireAt(cmd[1], *at, now) ? 1 : 0);
    }

    // Store is ms-only; seconds rounding lives here, matching Redis's (ms + 500) / 1000.
    void cmdTtl(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      const std::int64_t ms{store.ttlMs(cmd[1], now)};
      reply::integer(out, ms < 0 ? ms : (ms + kMsPerSec / 2) / kMsPerSec);
    }

    void cmdGet(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      // The view dies on the next Store call; bulk() copies it out before that.
      const Entry* e{store.find(cmd[1], now)};
      if (e == nullptr) {
        reply::nullBulk(out);
        return;
      }
      reply::bulk(out, e->value());
    }

    void cmdDel(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      std::int64_t n{0};
      for (std::size_t i{1}; i < cmd.argc; ++i) {
        n += store.erase(cmd[i], now) ? 1 : 0;
      }
      reply::integer(out, n);
    }

    // Repeated keys count once per mention, as in Redis. Not a use, so no LRU touch.
    void cmdExists(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
      std::int64_t n{0};
      for (std::size_t i{1}; i < cmd.argc; ++i) {
        n += store.find(cmd[i], now, /*touch=*/false) != nullptr ? 1 : 0;
      }
      reply::integer(out, n);
    }

    void cmdPing(const Command& cmd, Buffer& out) {
      if (cmd.argc > 2) {
        wrongArity("ping", out);
      } else if (cmd.argc == 2) {
        reply::bulk(out, cmd[1]);
      } else {
        reply::simpleString(out, "PONG");
      }
    }

    // ASYNC/SYNC accepted for client compatibility; both flush synchronously.
    void cmdFlushall(Store& store, const Command& cmd, Buffer& out) {
      if (cmd.argc > 2 ||
          (cmd.argc == 2 && !iequals(cmd[1], "async") && !iequals(cmd[1], "sync"))) {
        reply::error(out, kSyntax);
        return;
      }
      // TODO: Extend async
      store.clear();
      reply::simpleString(out, "OK");
    }

  }  // namespace

  DispatchResult dispatch(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
    if (cmd.argc == 0) {
      return DispatchResult::kContinue;
    }

    const Spec* spec{lookup(cmd[0])};
    if (spec == nullptr) {
      unknownCommand(cmd, out);
      return DispatchResult::kContinue;
    }
    if (!arityOk(*spec, cmd.argc)) {
      wrongArity(spec->name, out);
      return DispatchResult::kContinue;
    }

    switch (spec->cmd) {
      case Cmd::kGet:
        cmdGet(store, cmd, now, out);
        break;
      case Cmd::kSet:
        cmdSet(store, cmd, now, out);
        break;
      case Cmd::kDel:
        cmdDel(store, cmd, now, out);
        break;
      case Cmd::kExists:
        cmdExists(store, cmd, now, out);
        break;
      case Cmd::kExpire:
        cmdExpire(store, cmd, now, out);
        break;
      case Cmd::kTtl:
        cmdTtl(store, cmd, now, out);
        break;
      case Cmd::kPing:
        cmdPing(cmd, out);
        break;
      case Cmd::kEcho:
        reply::bulk(out, cmd[1]);
        break;
      case Cmd::kDbsize:
        reply::integer(out, static_cast<std::int64_t>(store.size()));
        break;
      case Cmd::kFlushall:
        cmdFlushall(store, cmd, out);
        break;
      case Cmd::kQuit:
        reply::simpleString(out, "OK");
        return DispatchResult::kCloseConnection;
    }
    return DispatchResult::kContinue;
  }

}  // namespace tinyredis
