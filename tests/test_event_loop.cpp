#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <functional>
#include <system_error>
#include <utility>

#include "net/event_loop.h"
#include "net/unique_fd.h"

using tinyredis::EventLoop;
using tinyredis::IoHandler;
using tinyredis::UniqueFd;

namespace {
  constexpr int kTimeoutMs{10};

  class SocketPair {
   public:
    SocketPair() {
      std::array<int, 2> fds{};
      if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds.data()) == -1) {
        throw std::system_error(errno, std::generic_category(), "socketpair");
      }
      local_.reset(fds[0]);
      peer_.reset(fds[1]);
    }

    // The handler under test owns the local end, so it is handed over rather than shared.
    [[nodiscard]] UniqueFd takeLocal() noexcept { return UniqueFd{local_.release()}; }

    void send() const {
      constexpr char kByte{'x'};
      ASSERT_EQ(::write(peer_.get(), &kByte, 1), 1);
    }

    void closePeer() noexcept { peer_.reset(); }

   private:
    UniqueFd local_{-1};
    UniqueFd peer_{-1};
  };

  class Probe final : public IoHandler {
   public:
    explicit Probe(UniqueFd fd, bool drain = true) noexcept
        : IoHandler(std::move(fd)), drain_(drain) {}

    void onReadable() override {
      ++reads;
      if (drain_) {
        drain();
      }
      if (onRead) {
        onRead();
      }
    }

    void onWritable() override { ++writes; }

    int reads{};
    int writes{};
    bool sawEof{};
    std::function<void()> onRead;

   private:
    void drain() {
      std::array<char, 64> buf{};
      while (true) {
        const ssize_t n{::read(fd(), buf.data(), buf.size())};
        if (n == 0) {
          sawEof = true;
        }
        if (n <= 0) {
          return;
        }
      }
    }

    bool drain_;
  };

  // Returns how many times onTick fired.
  int runTicks(EventLoop& loop, int ticks) {
    int seen{};
    loop.run(kTimeoutMs, [&] {
      if (++seen >= ticks) {
        loop.stop();
      }
    });
    return seen;
  }
}  // namespace

TEST(EventLoop, ReadableFiresOnData) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);
  sp.send();
  runTicks(loop, 1);

  EXPECT_EQ(p.reads, 1);
  EXPECT_EQ(p.writes, 0);
}

TEST(EventLoop, WritableFiresOnFreshSocket) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLOUT);
  runTicks(loop, 1);

  EXPECT_EQ(p.writes, 1);
  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, IdleSocketNeverFires) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);
  runTicks(loop, 3);

  EXPECT_EQ(p.reads, 0);
  EXPECT_EQ(p.writes, 0);
}

TEST(EventLoop, TimeoutStillTicks) {
  EventLoop loop;

  EXPECT_EQ(runTicks(loop, 3), 3);
  EXPECT_EQ(loop.tick(), 3U);
}

TEST(EventLoop, ModifySwapsInterest) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);
  loop.modify(p, EPOLLOUT);
  sp.send();
  runTicks(loop, 1);

  EXPECT_EQ(p.writes, 1);
  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, RemovedHandlerNeverFires) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);
  sp.send();
  loop.remove(p);
  runTicks(loop, 3);

  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, AddTwiceThrows) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);

  EXPECT_THROW(loop.add(p, EPOLLIN), std::system_error);
}

TEST(EventLoop, ModifyUnregisteredThrows) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  EXPECT_THROW(loop.modify(p, EPOLLIN), std::system_error);
}

TEST(EventLoop, OneTickPerBatch) {
  SocketPair a;
  SocketPair b;
  SocketPair c;
  EventLoop loop;
  Probe pa{a.takeLocal()};
  Probe pb{b.takeLocal()};
  Probe pc{c.takeLocal()};

  loop.add(pa, EPOLLIN);
  loop.add(pb, EPOLLIN);
  loop.add(pc, EPOLLIN);
  a.send();
  b.send();
  c.send();

  EXPECT_EQ(runTicks(loop, 1), 1);
  EXPECT_EQ(loop.tick(), 1U);
  EXPECT_EQ(pa.reads, 1);
  EXPECT_EQ(pb.reads, 1);
  EXPECT_EQ(pc.reads, 1);
}

TEST(EventLoop, UndrainedSocketFiresAgain) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal(), false};

  loop.add(p, EPOLLIN);
  sp.send();
  runTicks(loop, 3);

  EXPECT_EQ(p.reads, 3);
}

TEST(EventLoop, PeerCloseIsReadable) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.takeLocal()};

  loop.add(p, EPOLLIN);
  sp.closePeer();
  runTicks(loop, 1);

  EXPECT_EQ(p.reads, 1);
  EXPECT_TRUE(p.sawEof);
}

TEST(EventLoop, StopFinishesBatch) {
  SocketPair a;
  SocketPair b;
  EventLoop loop;
  Probe pa{a.takeLocal()};
  Probe pb{b.takeLocal()};
  pa.onRead = [&] { loop.stop(); };
  pb.onRead = [&] { loop.stop(); };

  loop.add(pa, EPOLLIN);
  loop.add(pb, EPOLLIN);
  a.send();
  b.send();

  EXPECT_EQ(runTicks(loop, 5), 1);
  EXPECT_EQ(pa.reads, 1);
  EXPECT_EQ(pb.reads, 1);
}
