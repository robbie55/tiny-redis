#include "core/command.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <string>

#include "core/buffer.h"
#include "core/clock.h"
#include "core/entry.h"
#include "core/store.h"

namespace {
  // Core commands: GET, SET, DEL, EXISTS, EXPIRE, TTL, PING. ECHO, DBSIZE, FLUSHALL and
  inline constexpr std::string kSetStr{"set"};
  inline constexpr std::string kGetStr{"get"};
  inline constexpr std::string kDelStr{"del"};
  inline constexpr std::string kExistsStr{"exists"};
  inline constexpr std::string kExpireStr{"expire"};
  inline constexpr std::string kTtlStr{"ttl"};
  inline constexpr std::string kPingStr{"ping"};

  inline constexpr std::string kEX{"EX"};
  inline constexpr std::string kPX{"PX"};
  inline constexpr std::string kKeepTtlStr{"KEEPTTL"};

  using Args = std::array<std::string_view, tinyredis::kMaxArgs>;
  using namespace tinyredis;

  enum class CommandType : std::uint8_t {
    kSet,
    kGet,
    kDel,
    kExists,
    kExpire,
    kTtl,
    kPing,
    kUnknown
  };

  bool strCompare(const std::string& str1, const std::string& str2) {
    return std::ranges::equal(
        str1, str2, [](unsigned char a, unsigned char b) { return std::tolower(a) == b; });
  }

  CommandType getCommandType(std::string_view arg) {
    if (strCompare(std::string{arg}, kSetStr)) {
      return CommandType::kSet;
    }
    if (strCompare(std::string{arg}, kGetStr)) {
      return CommandType::kGet;
    }
    if (strCompare(std::string{arg}, kDelStr)) {
      return CommandType::kDel;
    }
    if (strCompare(std::string{arg}, kExistsStr)) {
      return CommandType::kExists;
    }
    if (strCompare(std::string{arg}, kExpireStr)) {
      return CommandType::kExpire;
    }
    if (strCompare(std::string{arg}, kTtlStr)) {
      return CommandType::kTtl;
    }
    if (strCompare(std::string{arg}, kPingStr)) {
      return CommandType::kPing;
    }

    return CommandType::kUnknown;
  }

  // v1 won't support negative arity, see store.h for command contracts
  std::int8_t getCommandArity(CommandType cmdType) {
    switch (cmdType) {
      case CommandType::kSet:
        return -3;
      case CommandType::kExpire:
        return 3;
      case CommandType::kGet:
      case CommandType::kTtl:
      case CommandType::kExists:
      case CommandType::kDel:
        return 2;
      case CommandType::kPing:
        return 1;
      case CommandType::kUnknown:
        break;
    }

    return 0;
  }

  std::string unknownErr(std::string_view cmd) {
    return std::string{"ERR unknown command '"} + std::string{cmd} + "'";
  }

  bool tryParseSv(std::string_view sv, std::int64_t& value) {
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

    // Check if the entire string_view was parsed successfully without overflow
    return ec == std::errc{} && ptr == sv.data() + sv.size();
  }

  DispatchResult dispatchSet(Store& store, const Args& args, std::int64_t now, Buffer& out) {
    const std::string_view key{args[1]};
    const std::string_view value{args[2]};

    if (args.size() == 3) {
      store.set(key, value, kNoExpiry, now);
      out.append("OK");
      return DispatchResult::kContinue;
    }

    if (args[3] == kKeepTtlStr) {
      store.set(key, value, kKeepTtl, now);
      out.append("OK");
      return DispatchResult::kContinue;
    }

    // if args don't include KEEPTTL, only options are time specifiers EX or PX with corresponding
    // time, if time is missing, syntax err
    if (args.size() < 5) {
      out.append("ERR syntax error");
      return DispatchResult::kContinue;
    }

    const std::string_view timeSpecifier{args[3]};
    std::int64_t time{};
    if (!tryParseSv(args[4], time)) {
      out.append("ERR invalid expire time");
      return DispatchResult::kContinue;
    }

    if (timeSpecifier == kEX) {
      time *= 1000;
    }

    store.set(key, value, time, now);
    out.append("OK");
    return DispatchResult::kContinue;
  }

  DispatchResult dispatchExpire(Store& store, const Args& args, std::int64_t now, Buffer& out) {
    const std::string_view svMilliseconds{args[2]};
    std::int64_t milliseconds{};
    if (!tryParseSv(svMilliseconds, milliseconds)) {
      out.append("ERR invalid expire time");
      return DispatchResult::kContinue;
    }

    store.setExpireAt(args[1], milliseconds, now);
    return DispatchResult::kContinue;
  }

  DispatchResult dispatchGet(Store& store, const Args& args, std::int64_t now, Buffer& out) {
    Entry* e{store.find(args[1], now)};
    out.append(e == nullptr ? "nil" : e->value());
    return DispatchResult::kContinue;
  }

  DispatchResult dispatchExists(Store& store, const Args& args, std::int64_t now, Buffer& out) {
    Entry* e{store.find(args[1], now)};
    out.append(e == nullptr ? "0" : "1");
    return DispatchResult::kContinue;
  }

  DispatchResult dispatchDel(Store& store, const Args& args, std::int64_t now, Buffer& out) {
    Entry* e{store.find(args[1], now)};
    if (e == nullptr) {
      out.append("0");
    } else {
      store.erase(args[1], now);
      out.append("1");
    }
    return DispatchResult::kContinue;
  }
}  // namespace

namespace tinyredis {
  DispatchResult dispatch(Store& store, const Command& cmd, std::int64_t now, Buffer& out) {
    if (cmd.argc == 0) {
      return DispatchResult::kContinue;
    }

    CommandType cmdType{getCommandType(cmd.argv[0])};
    if (cmdType == CommandType::kUnknown) {
      out.append(unknownErr(cmd.argv[0]));
      return DispatchResult::kContinue;
    }

    std::int8_t arity{getCommandArity(cmdType)};

    if ((arity < 0 && cmd.argc < static_cast<std::size_t>(std::abs(arity))) ||
        cmd.argc != static_cast<std::size_t>(arity)) {
      out.append("ERR wrong number of arguments");
      return DispatchResult::kContinue;
    }

    switch (cmdType) {
      case CommandType::kSet:
        return dispatchSet(store, cmd.argv, now, out);
        break;
      case CommandType::kExpire: {
        return dispatchExpire(store, cmd.argv, now, out);
      }
      case CommandType::kGet: {
        return dispatchGet(store, cmd.argv, now, out);
      }
      case CommandType::kTtl: {
        store.ttlMs(cmd.argv[1], now);
        return DispatchResult::kContinue;
      }
      case CommandType::kExists: {
        return dispatchExists(store, cmd.argv, now, out);
      }
      case CommandType::kDel: {
        return dispatchDel(store, cmd.argv, now, out);
      }
      case CommandType::kPing:
        out.append("PONG");
        return DispatchResult::kContinue;
      case CommandType::kUnknown:
        return DispatchResult::kCloseConnection;
        break;
    }

    return DispatchResult::kCloseConnection;
  }

}  // namespace tinyredis
