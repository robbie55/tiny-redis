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
      return fullRun(args, now).first;
    }

    std::pair<std::string, DispatchResult> fullRun(std::initializer_list<std::string_view> args,
                                                   std::int64_t now) {
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
  Harness h{};
  EXPECT_EQ(h.run({"PING"}), "+PONG\r\n");
}

TEST(Commands, SetAndGet) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, SetWithExpiry) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, SetWithPxAndKeepTtl) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, SetRejectsBadExpiry) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, DelAndExists) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, ExpireAndTtl) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, NonPositiveExpireDeletesTheKey) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, DbsizeAndFlushall) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, QuitAsksForClose) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Commands, UnknownCommandAndArityErrors) { GTEST_SKIP() << "TODO(robbie): not written yet"; }
