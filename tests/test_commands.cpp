#include <gtest/gtest.h>

#include <initializer_list>

#include "core/command.h"
#include "core/resp.h"
#include "core/store.h"

using tinyredis::Buffer;
using tinyredis::Command;
using tinyredis::DispatchResult;
using tinyredis::Store;

namespace {
  constexpr std::uint64_t kSeed{0x5eed'1234'abcd'0004ULL};
  constexpr std::int64_t kNow{1'000'000};

  class Harness {
   public:
    Harness(Store::Config cfg = {.seed = kSeed}) : store_{cfg} {}

    std::string run(std::initializer_list<std::string_view> args, std::int64_t now = kNow) {
      return runFull(args, now).first;
    }

    std::pair<std::string, DispatchResult> runFull(std::initializer_list<std::string_view> args,
                                                   std::int64_t now = kNow) {
      std::vector<std::string> owned{args.begin(), args.end()};

      Command cmd{};

      for (const std::string& arg : owned) {
        cmd.argv[cmd.argc++] = arg;
      }
      Buffer out{};
      const DispatchResult res{tinyredis::dispatch(store_, cmd, now, out)};

      return {std::string{out.readable()}, res};
    }

   private:
    Store store_;
  };
}  // namespace

TEST(Commands, Ping) {
  Harness h;
  EXPECT_EQ(h.run({"PING"}), "+PONG\r\n");
  EXPECT_EQ(h.run({"ping", "hi"}), "$2\r\nhi\r\n");
  EXPECT_EQ(h.run({"PING", "a", "b"}), "-ERR wrong number of arguments for 'ping' command\r\n");
}

TEST(Commands, SetAndGet) {
  Harness h;
  EXPECT_EQ(h.run({"GET", "k"}), "$-1\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v"}), "+OK\r\n");
  EXPECT_EQ(h.run({"get", "k"}), "$1\r\nv\r\n");
  EXPECT_EQ(h.run({"sEt", "k", "longer value"}), "+OK\r\n");
  EXPECT_EQ(h.run({"GeT", "k"}), "$12\r\nlonger value\r\n");
}

TEST(Commands, SetIsBinarySafe) {
  Harness h;
  const std::string v{"a\r\nb\0c", 6};
  EXPECT_EQ(h.run({"SET", "k", v}), "+OK\r\n");
  EXPECT_EQ(h.run({"GET", "k"}), "$6\r\n" + v + "\r\n");
}

TEST(Commands, SetWithExpiry) {
  Harness h;
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "10"}), "+OK\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}), ":10\r\n");
  EXPECT_EQ(h.run({"GET", "k"}, kNow + 10'000), "$1\r\nv\r\n");
  EXPECT_EQ(h.run({"GET", "k"}, kNow + 10'001), "$-1\r\n");
}

TEST(Commands, BareSetClearsTtl) {
  Harness h;
  h.run({"SET", "k", "v", "EX", "10"});
  EXPECT_EQ(h.run({"SET", "k", "w"}), "+OK\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}), ":-1\r\n");
}

TEST(Commands, SetWithPxAndKeepTtl) {
  Harness h;
  EXPECT_EQ(h.run({"SET", "k", "v", "px", "1500"}), "+OK\r\n");
  EXPECT_EQ(h.run({"SET", "k", "w", "KeepTtl"}), "+OK\r\n");
  EXPECT_EQ(h.run({"GET", "k"}), "$1\r\nw\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}), ":2\r\n");  // 1500 ms rounds half-up
  EXPECT_EQ(h.run({"GET", "k"}, kNow + 1'501), "$-1\r\n");

  EXPECT_EQ(h.run({"SET", "fresh", "v", "KEEPTTL"}), "+OK\r\n");
  EXPECT_EQ(h.run({"TTL", "fresh"}), ":-1\r\n");
}

TEST(Commands, SetRejectsBadExpiry) {
  Harness h;
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "0"}), "-ERR invalid expire time in 'set' command\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "PX", "-5"}), "-ERR invalid expire time in 'set' command\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "9223372036854775807"}),
            "-ERR invalid expire time in 'set' command\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "ten"}),
            "-ERR value is not an integer or out of range\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "+1"}),
            "-ERR value is not an integer or out of range\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX"}), "-ERR syntax error\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "1", "PX", "1"}), "-ERR syntax error\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "EX", "1", "KEEPTTL"}), "-ERR syntax error\r\n");
  EXPECT_EQ(h.run({"SET", "k", "v", "NX"}), "-ERR syntax error\r\n");
  EXPECT_EQ(h.run({"EXISTS", "k"}), ":0\r\n");  // no rejected SET may write
}

TEST(Commands, SetWhenFullRepliesOom) {
  Harness h{{.maxKeys = 1, .seed = kSeed}};
  EXPECT_EQ(h.run({"SET", "a", "v"}), "+OK\r\n");
  EXPECT_EQ(h.run({"SET", "b", "v"}),
            "-OOM command not allowed when used memory > 'maxmemory'.\r\n");
  EXPECT_EQ(h.run({"SET", "a", "w"}), "+OK\r\n");  // overwrite needs no slot
}

TEST(Commands, DelAndExists) {
  Harness h;
  h.run({"SET", "a", "1"});
  h.run({"SET", "b", "2"});
  EXPECT_EQ(h.run({"EXISTS", "a", "a", "nope"}), ":2\r\n");
  EXPECT_EQ(h.run({"DEL", "a", "nope"}), ":1\r\n");
  EXPECT_EQ(h.run({"DEL", "a"}), ":0\r\n");
  EXPECT_EQ(h.run({"EXISTS", "a"}), ":0\r\n");
  EXPECT_EQ(h.run({"EXISTS", "b"}), ":1\r\n");
}

TEST(Commands, ExpireAndTtl) {
  Harness h;
  EXPECT_EQ(h.run({"TTL", "k"}), ":-2\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "k", "10"}), ":0\r\n");
  h.run({"SET", "k", "v"});
  EXPECT_EQ(h.run({"TTL", "k"}), ":-1\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "k", "10"}), ":1\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}), ":10\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}, kNow + 9'499), ":1\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}, kNow + 9'501), ":0\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}, kNow + 10'001), ":-2\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "k", "x"}), "-ERR value is not an integer or out of range\r\n");
  h.run({"SET", "k", "v"});
  EXPECT_EQ(h.run({"EXPIRE", "k", "9223372036854775807"}),
            "-ERR invalid expire time in 'expire' command\r\n");
  EXPECT_EQ(h.run({"TTL", "k"}), ":-1\r\n");
}

TEST(Commands, NonPositiveExpireDeletesTheKey) {
  Harness h;
  h.run({"SET", "a", "v"});
  h.run({"SET", "b", "v"});
  EXPECT_EQ(h.run({"EXPIRE", "a", "0"}), ":1\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "b", "-100"}), ":1\r\n");
  EXPECT_EQ(h.run({"EXISTS", "a", "b"}), ":0\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "a", "0"}), ":0\r\n");
}

TEST(Commands, EchoDbsizeAndFlushall) {
  Harness h;
  EXPECT_EQ(h.run({"ECHO", "hello"}), "$5\r\nhello\r\n");
  EXPECT_EQ(h.run({"DBSIZE"}), ":0\r\n");
  h.run({"SET", "a", "1"});
  h.run({"SET", "b", "2"});
  EXPECT_EQ(h.run({"DBSIZE"}), ":2\r\n");
  EXPECT_EQ(h.run({"FLUSHALL", "bogus"}), "-ERR syntax error\r\n");
  EXPECT_EQ(h.run({"DBSIZE"}), ":2\r\n");
  EXPECT_EQ(h.run({"flushall", "ASYNC"}), "+OK\r\n");
  EXPECT_EQ(h.run({"DBSIZE"}), ":0\r\n");
  EXPECT_EQ(h.run({"GET", "a"}), "$-1\r\n");
}

TEST(Commands, QuitAsksForClose) {
  Harness h;
  const auto [bytes, r]{h.runFull({"QUIT"})};
  EXPECT_EQ(bytes, "+OK\r\n");
  EXPECT_EQ(r, DispatchResult::kCloseConnection);
  EXPECT_EQ(h.runFull({"PING"}).second, DispatchResult::kContinue);
}

TEST(Commands, EmptyCommandIsANoOp) {
  Harness h;
  const auto [bytes, r]{h.runFull({})};
  EXPECT_EQ(bytes, "");
  EXPECT_EQ(r, DispatchResult::kContinue);
}

TEST(Commands, UnknownCommandAndArityErrors) {
  Harness h;
  EXPECT_EQ(h.run({"FOO"}), "-ERR unknown command 'FOO', with args beginning with: \r\n");
  EXPECT_EQ(h.run({"foo", "a", "b"}),
            "-ERR unknown command 'foo', with args beginning with: 'a' 'b' \r\n");
  EXPECT_EQ(h.run({"GET"}), "-ERR wrong number of arguments for 'get' command\r\n");
  EXPECT_EQ(h.run({"GET", "a", "b"}), "-ERR wrong number of arguments for 'get' command\r\n");
  EXPECT_EQ(h.run({"SET", "k"}), "-ERR wrong number of arguments for 'set' command\r\n");
  EXPECT_EQ(h.run({"DEL"}), "-ERR wrong number of arguments for 'del' command\r\n");
  EXPECT_EQ(h.run({"EXPIRE", "k"}), "-ERR wrong number of arguments for 'expire' command\r\n");
  EXPECT_EQ(h.run({"DBSIZE", "x"}), "-ERR wrong number of arguments for 'dbsize' command\r\n");
}

TEST(Commands, UnknownCommandErrorCannotBreakFraming) {
  Harness h;
  EXPECT_EQ(h.run({"x\r\n+OK", "a\nb"}),
            "-ERR unknown command 'x  +OK', with args beginning with: 'a b' \r\n");
}
