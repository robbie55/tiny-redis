#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <functional>
#include <system_error>

#include "net/event_loop.h"
#include "net/unique_fd.h"

using tinyredis::EventLoop;
using tinyredis::IoHandler;

namespace {
  constexpr int kTimeoutMs{10};

  class SocketPair {
   public:
    SocketPair() {
      std::array<int, 2> fds{};
      if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds.data()) == -1) {
        throw std::system_error(errno, std::generic_category(), "socketpair");
      }
      local_ = fds[0];
      peer_ = fds[1];
    }

    ~SocketPair() {
      closeFd(local_);
      closeFd(peer_);
    }

    SocketPair(const SocketPair&) = delete;
    SocketPair& operator=(const SocketPair&) = delete;
    SocketPair(SocketPair&&) = delete;
    SocketPair& operator=(SocketPair&&) = delete;

    [[nodiscard]] int local() const noexcept { return local_; }

    void send() const {
      constexpr char kByte{'x'};
      ASSERT_EQ(::write(peer_, &kByte, 1), 1);
    }

    void closePeer() noexcept { closeFd(peer_); }

   private:
    static void closeFd(int& fd) noexcept {
      if (fd != -1) {
        ::close(fd);
        fd = -1;
      }
    }

    int local_{-1};
    int peer_{-1};
  };

  class Probe final : public IoHandler {
   public:
    explicit Probe(int fd, bool drain = true) noexcept
        : IoHandler(tinyredis::UniqueFd{fd}), drain_(drain) {}

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
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLIN);
  sp.send();
  runTicks(loop, 1);

  EXPECT_EQ(p.reads, 1);
  EXPECT_EQ(p.writes, 0);
}

TEST(EventLoop, WritableFiresOnFreshSocket) {
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLOUT);
  runTicks(loop, 1);

  EXPECT_EQ(p.writes, 1);
  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, IdleSocketNeverFires) {
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

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
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLIN);
  loop.modify(p, EPOLLOUT);
  sp.send();
  runTicks(loop, 1);

  EXPECT_EQ(p.writes, 1);
  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, RemovedHandlerNeverFires) {
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLIN);
  sp.send();
  loop.remove(p);
  runTicks(loop, 3);

  EXPECT_EQ(p.reads, 0);
}

TEST(EventLoop, AddTwiceThrows) {
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLIN);

  EXPECT_THROW(loop.add(p, EPOLLIN), std::system_error);
}

TEST(EventLoop, ModifyUnregisteredThrows) {
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  EXPECT_THROW(loop.modify(p, EPOLLIN), std::system_error);
}

TEST(EventLoop, OneTickPerBatch) {
  const SocketPair a;
  const SocketPair b;
  const SocketPair c;
  EventLoop loop;
  Probe pa{a.local()};
  Probe pb{b.local()};
  Probe pc{c.local()};

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
  const SocketPair sp;
  EventLoop loop;
  Probe p{sp.local(), false};

  loop.add(p, EPOLLIN);
  sp.send();
  runTicks(loop, 3);

  EXPECT_EQ(p.reads, 3);
}

TEST(EventLoop, PeerCloseIsReadable) {
  SocketPair sp;
  EventLoop loop;
  Probe p{sp.local()};

  loop.add(p, EPOLLIN);
  sp.closePeer();
  runTicks(loop, 1);

  EXPECT_EQ(p.reads, 1);
  EXPECT_TRUE(p.sawEof);
}

TEST(EventLoop, StopFinishesBatch) {
  const SocketPair a;
  const SocketPair b;
  EventLoop loop;
  Probe pa{a.local()};
  Probe pb{b.local()};
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
