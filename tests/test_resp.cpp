#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/buffer.h"
#include "core/resp.h"

using tinyredis::Buffer;
using tinyredis::Command;
using tinyredis::Error;
using tinyredis::errorToString;
using tinyredis::kMaxInlineLength;
using tinyredis::parseCommand;
using tinyredis::ParseResult;
using tinyredis::ParseStatus;

// SET foo bar, as redis-cli and redis-benchmark send it.
inline constexpr std::string_view kArrayValid{"*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"};

// Count 17 exceeds kMaxArgs.
inline constexpr std::string_view kArrayInvalid{"*17\r\n$4\r\nMSET\r\n"};

inline constexpr std::string_view kBulkValid{"*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n"};

// $3 declares three bytes, but "!" sits where the CRLF trailer belongs.
inline constexpr std::string_view kBulkInvalid{"*2\r\n$3\r\nGET\r\n$3\r\nfoo!\r\n"};

inline constexpr std::string_view kInlineValid{"PING\r\n"};

// 17 tokens, one past kMaxArgs.
inline constexpr std::string_view kInlineInvalid{"A B C D E F G H I J K L M N O P Q\r\n"};

namespace {

  void expectOk(const ParseResult& res, const Command& cmd, std::size_t consumed,
                std::size_t argc) {
    EXPECT_EQ(res.status, ParseStatus::kOk);
    EXPECT_EQ(res.consumed, consumed);
    EXPECT_EQ(cmd.argc, argc);
  }

  void expectIncomplete(const ParseResult& res, const Command& cmd) {
    EXPECT_EQ(res.status, ParseStatus::kIncomplete);
    EXPECT_EQ(res.consumed, 0U);
    EXPECT_EQ(cmd.argc, 0U);
  }

  void expectProtocolError(const ParseResult& res, const Command& cmd) {
    EXPECT_EQ(res.status, ParseStatus::kProtocolError);
    EXPECT_FALSE(res.error.empty());
    EXPECT_EQ(cmd.argc, 0U);
  }

  void expectTailCleared(const Command& cmd) {
    for (std::size_t i = cmd.argc; i < tinyredis::kMaxArgs; ++i) {
      SCOPED_TRACE(i);
      EXPECT_TRUE(cmd.argv[i].empty());
    }
  }

}  // namespace

TEST(RespParser, ParsesArrayOfBulkStrings) {
  Command cmd{};
  const ParseResult res{parseCommand(kArrayValid, cmd)};

  expectOk(res, cmd, kArrayValid.size(), 3);
  EXPECT_EQ(cmd[0], "SET");
  EXPECT_EQ(cmd[1], "foo");
  EXPECT_EQ(cmd[2], "bar");
  expectTailCleared(cmd);

  // argv views point into the input, not copies.
  EXPECT_GE(cmd[0].data(), kArrayValid.data());
  EXPECT_LE(cmd[2].data() + cmd[2].size(), kArrayValid.data() + kArrayValid.size());
}

TEST(RespParser, ReportsIncompleteForEveryPrefix) {
  // TCP can split a command at any byte.
  for (const std::string_view whole : {kArrayValid, kBulkValid, kInlineValid}) {
    for (std::size_t i = 0; i < whole.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "prefix length " << i << " of " << whole.size());
      Command cmd{};
      const ParseResult res{parseCommand(whole.substr(0, i), cmd)};
      expectIncomplete(res, cmd);
      expectTailCleared(cmd);
    }

    Command cmd{};
    const ParseResult res{parseCommand(whole, cmd)};
    EXPECT_EQ(res.status, ParseStatus::kOk);
    EXPECT_EQ(res.consumed, whole.size());
  }
}

TEST(RespParser, ConsumesOneCommandAtATimeFromAPipelinedBatch) {
  // Wire forms mix freely within one batch.
  const std::string batch{std::string{kArrayValid} + std::string{kBulkValid} +
                          std::string{kInlineValid}};
  std::string_view rest{batch};

  Command cmd{};
  ParseResult res{parseCommand(rest, cmd)};
  expectOk(res, cmd, kArrayValid.size(), 3);
  EXPECT_EQ(cmd[0], "SET");
  rest.remove_prefix(res.consumed);

  res = parseCommand(rest, cmd);
  expectOk(res, cmd, kBulkValid.size(), 2);
  EXPECT_EQ(cmd[0], "GET");
  EXPECT_EQ(cmd[1], "foo");
  expectTailCleared(cmd);
  rest.remove_prefix(res.consumed);

  res = parseCommand(rest, cmd);
  expectOk(res, cmd, kInlineValid.size(), 1);
  EXPECT_EQ(cmd[0], "PING");
  expectTailCleared(cmd);
  rest.remove_prefix(res.consumed);

  EXPECT_TRUE(rest.empty());
  res = parseCommand(rest, cmd);
  expectIncomplete(res, cmd);
}

TEST(RespParser, HandlesBinarySafeBulkStrings) {
  // NUL, CRLF and RESP type bytes are all legal payload.
  const std::string value{"a\0b\r\nc*$", 8};
  const std::string frame{"*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$" + std::to_string(value.size()) +
                          "\r\n" + value + "\r\n"};

  Command cmd{};
  const ParseResult res{parseCommand(frame, cmd)};

  expectOk(res, cmd, frame.size(), 3);
  EXPECT_EQ(cmd[1], "key");
  EXPECT_EQ(cmd[2].size(), value.size());
  EXPECT_EQ(cmd[2], std::string_view(value));
}

TEST(RespParser, EmptyBulkStringIsValid) {
  // An empty argument still counts toward argc.
  constexpr std::string_view kFrame{"*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$0\r\n\r\n"};

  Command cmd{};
  const ParseResult res{parseCommand(kFrame, cmd)};

  expectOk(res, cmd, kFrame.size(), 3);
  EXPECT_EQ(cmd[1], "key");
  EXPECT_TRUE(cmd[2].empty());
}

TEST(RespParser, EmptyArrayIsAConsumedNoOp) {
  // Incomplete would stall the read loop and an error would drop the connection.
  Command empty{};
  const ParseResult emptyRes{parseCommand("*0\r\n", empty)};
  expectOk(emptyRes, empty, 4, 0);
  expectTailCleared(empty);

  // A blank inline line is the same no-op.
  Command cmd{};
  const ParseResult res{parseCommand("\r\n", cmd)};
  expectOk(res, cmd, 2, 0);
}

TEST(RespParser, AcceptsInlineCommands) {
  Command cmd{};
  const ParseResult res{parseCommand(kInlineValid, cmd)};

  expectOk(res, cmd, kInlineValid.size(), 1);
  EXPECT_EQ(cmd[0], "PING");
  expectTailCleared(cmd);

  // Any first byte other than '*' means inline, even a reply type byte. Dispatch rejects
  // "+OK" as an unknown command.
  Command other{};
  const ParseResult res2{parseCommand("+OK\r\n", other)};
  expectOk(res2, other, 5, 1);
  EXPECT_EQ(other[0], "+OK");
}

TEST(RespParser, InlineCommandSplitsOnWhitespace) {
  // Leading, trailing and repeated separators collapse. Tabs count as separators.
  constexpr std::string_view kFrame{"  SET \t  foo   bar  \r\n"};

  Command cmd{};
  const ParseResult res{parseCommand(kFrame, cmd)};

  expectOk(res, cmd, kFrame.size(), 3);
  EXPECT_EQ(cmd[0], "SET");
  EXPECT_EQ(cmd[1], "foo");
  EXPECT_EQ(cmd[2], "bar");
  expectTailCleared(cmd);
}

TEST(RespParser, InlineCommandAcceptsBareLf) {
  // nc sends a bare LF unless run with -C. Redis accepts it and strips a trailing CR.
  constexpr std::string_view kFrame{"SET foo bar\n"};

  Command cmd{};
  const ParseResult res{parseCommand(kFrame, cmd)};

  expectOk(res, cmd, kFrame.size(), 3);
  EXPECT_EQ(cmd[0], "SET");
  EXPECT_EQ(cmd[1], "foo");
  EXPECT_EQ(cmd[2], "bar");
  expectTailCleared(cmd);

  // Both terminators in one batch. The CR never leaks into the last token.
  std::string_view rest{"PING\nPING\r\n"};

  ParseResult step{parseCommand(rest, cmd)};
  expectOk(step, cmd, 5, 1);
  EXPECT_EQ(cmd[0], "PING");
  rest.remove_prefix(step.consumed);

  step = parseCommand(rest, cmd);
  expectOk(step, cmd, 6, 1);
  EXPECT_EQ(cmd[0], "PING");

  // A CR with no LF yet could still become CRLF.
  Command pending{};
  expectIncomplete(parseCommand("PING\r", pending), pending);

  // The array form stays strict CRLF.
  Command strict{};
  expectProtocolError(parseCommand("*1\n$4\r\nPING\r\n", strict), strict);
}

TEST(RespParser, RejectsMalformedInput) {
  const std::vector<std::pair<std::string, std::string>> cases{
      {std::string{kArrayInvalid}, "multibulk count exceeds kMaxArgs"},
      {"*abc\r\n", "multibulk count is not a number"},
      {"*-1\r\n", "negative multibulk count; readInt is unsigned"},
      {"*\r\n", "multibulk count is empty"},
      {"*99999999999999999999\r\n", "multibulk count overflows size_t"},
      {"*1\r\n+OK\r\n", "element header is not a bulk string"},
      {"*1\r\n$-1\r\n", "null bulk is legal in a reply, never in a request"},
      {"*1\r\n$abc\r\n", "bulk length is not a number"},
      {std::string{kInlineInvalid}, "inline token count exceeds kMaxArgs"},
      {std::string(kMaxInlineLength + 1, 'a'), "unterminated inline line past kMaxInlineLength"},
  };

  for (const auto& [input, why] : cases) {
    SCOPED_TRACE(why);
    Command cmd{};
    const ParseResult res{parseCommand(input, cmd)};
    expectProtocolError(res, cmd);
    expectTailCleared(cmd);
  }

  // kArgLength is the argument count, kBulkLength one element, kInlineLength the inline line.
  const std::vector<std::pair<std::string, Error>> mapped{
      {"*17\r\n$4\r\nMSET\r\n", Error::kArgLength},
      {"*abc\r\n", Error::kArgLength},
      {"*\r\n", Error::kArgLength},
      {"*-1\r\n", Error::kArgLength},
      {"*99999999999999999999\r\n", Error::kArgLength},
      {"*1\r\n+OK\r\n", Error::kBulkLength},
      {"*1\r\n$-1\r\n", Error::kBulkLength},
      {"*1\r\n$abc\r\n", Error::kBulkLength},
      {"*1\r\n$3\r\nfoo!\r\n", Error::kBulkLength},
      {std::string(kMaxInlineLength + 1, 'a'), Error::kInlineLength},
  };

  for (const auto& [input, want] : mapped) {
    SCOPED_TRACE(input.substr(0, 24));
    Command cmd{};
    const ParseResult res{parseCommand(input, cmd)};
    EXPECT_EQ(res.status, ParseStatus::kProtocolError);
    EXPECT_EQ(res.error, errorToString(want));
  }
}

TEST(RespParser, RejectsBulkStringNotTerminatedByCrlf) {
  const std::vector<std::pair<std::string, std::string>> cases{
      {std::string{kBulkInvalid}, "byte after the payload is '!', not CR"},
      {"*1\r\n$3\r\nfoobar\r\n", "payload is longer than its declared length"},
      {"*1\r\n$3\r\nfoo\n\r\n", "terminator is a bare LF, not CRLF"},
  };

  for (const auto& [input, why] : cases) {
    SCOPED_TRACE(why);
    Command cmd{};
    const ParseResult res{parseCommand(input, cmd)};
    expectProtocolError(res, cmd);
  }
}

TEST(RespReply, EncodesEachType) {
  const auto encoded = [](auto&& fn) {
    Buffer out{};
    fn(out);
    return std::string{out.readable()};
  };

  namespace reply = tinyredis::reply;

  EXPECT_EQ(encoded([](Buffer& b) { reply::simpleString(b, "OK"); }), "+OK\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::simpleString(b, "PONG"); }), "+PONG\r\n");

  EXPECT_EQ(encoded([](Buffer& b) { reply::error(b, "ERR unknown command"); }),
            "-ERR unknown command\r\n");

  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, 0); }), ":0\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, 42); }), ":42\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, -2); }), ":-2\r\n");
  // TTL sentinels and the int64 extremes.
  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, -1); }), ":-1\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, INT64_MIN); }), ":-9223372036854775808\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::integer(b, INT64_MAX); }), ":9223372036854775807\r\n");

  EXPECT_EQ(encoded([](Buffer& b) { reply::bulk(b, "foo"); }), "$3\r\nfoo\r\n");

  const std::string binary{"a\0b\r\n", 5};
  EXPECT_EQ(encoded([&binary](Buffer& b) { reply::bulk(b, binary); }),
            std::string{"$5\r\n"} + binary + "\r\n");

  EXPECT_EQ(encoded([](Buffer& b) { reply::arrayHeader(b, 0); }), "*0\r\n");
  EXPECT_EQ(encoded([](Buffer& b) { reply::arrayHeader(b, 2); }), "*2\r\n");

  // A missing key and a key set to "" must encode differently.
  const std::string nullBulk{encoded([](Buffer& b) { reply::nullBulk(b); })};
  const std::string emptyBulk{encoded([](Buffer& b) { reply::bulk(b, ""); })};
  EXPECT_EQ(nullBulk, "$-1\r\n");
  EXPECT_EQ(emptyBulk, "$0\r\n\r\n");
  EXPECT_NE(nullBulk, emptyBulk);

  // Encoders append and never reset the buffer.
  Buffer out{};
  reply::simpleString(out, "OK");
  reply::integer(out, 7);
  reply::bulk(out, "hi");
  EXPECT_EQ(std::string{out.readable()}, "+OK\r\n:7\r\n$2\r\nhi\r\n");
}
