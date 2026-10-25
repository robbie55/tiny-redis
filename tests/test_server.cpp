#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/clock.h"
#include "net/event_loop.h"
#include "net/server.h"
#include "net/unique_fd.h"

using tinyredis::IoHandler;
using tinyredis::Server;
using tinyredis::UniqueFd;

namespace {
  constexpr int kRunMs{50};
  constexpr std::string_view kLoopback{"127.0.0.1"};

  [[noreturn]] void throwSys(const char* msg) {
    throw std::system_error(errno, std::generic_category(), msg);
  }

  // Server exposes no bound port, so ask the kernel for a free one. Racy in theory, but
  // nothing else on the box is grabbing ephemeral ports between here and bind.
  std::uint16_t freePort() {
    const UniqueFd fd{::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len{sizeof(addr)};
    if (::bind(fd.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1 ||
        ::getsockname(fd.get(), reinterpret_cast<sockaddr*>(&addr), &len) == -1) {
      throwSys("freePort");
    }
    return ntohs(addr.sin_port);
  }

  Server::Config testConfig() {
    Server::Config cfg{};
    cfg.bindAddr = std::string{kLoopback};
    cfg.port = freePort();
    cfg.expireIntervalMs = 10;
    return cfg;
  }

  // Calls stop() from inside the server's own loop, so tests stay single-threaded and
  // running_ is never touched from two threads.
  class StopTimer final : public IoHandler {
   public:
    StopTimer(Server& server, int afterMs)
        : IoHandler{UniqueFd{::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)}},
          server_{server} {
      if (fd() == -1) {
        throwSys("timerfd_create");
      }
      itimerspec spec{};
      spec.it_value.tv_sec = afterMs / 1000;
      spec.it_value.tv_nsec = static_cast<long>(afterMs % 1000) * 1'000'000L;
      if (::timerfd_settime(fd(), 0, &spec, nullptr) == -1) {
        throwSys("timerfd_settime");
      }
      server_.loop().add(*this, EPOLLIN);
    }

    ~StopTimer() override { server_.loop().remove(*this); }

    StopTimer(const StopTimer&) = delete;
    StopTimer& operator=(const StopTimer&) = delete;
    StopTimer(StopTimer&&) = delete;
    StopTimer& operator=(StopTimer&&) = delete;

    void onReadable() override {
      std::uint64_t expirations{};
      [[maybe_unused]] const ssize_t n{::read(fd(), &expirations, sizeof(expirations))};
      server_.stop();
    }

    void onWritable() override {}

   private:
    Server& server_;
  };

  void runFor(Server& server, int ms = kRunMs) {
    const StopTimer timer{server, ms};
    server.run();
  }

  // Blocking client socket. Reads use a short timeout so a missing reply fails the test
  // instead of hanging it.
  class Client {
   public:
    explicit Client(std::uint16_t port) : fd_{::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)} {
      if (!fd_) {
        throwSys("socket");
      }
      timeval tv{.tv_sec = 0, .tv_usec = 200'000};
      ::setsockopt(fd_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      // Completes against the listen backlog, so the server needn't be running yet.
      if (::connect(fd_.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        throwSys("connect");
      }
    }

    void send(std::string_view bytes) const {
      ASSERT_EQ(::write(fd_.get(), bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
    }

    // Everything the server has sent so far; stops at EOF or when nothing more arrives.
    std::string recvAll() {
      std::string got;
      std::array<char, 512> buf{};
      for (;;) {
        const ssize_t n{::read(fd_.get(), buf.data(), buf.size())};
        if (n == 0) {
          sawEof_ = true;
        }
        if (n <= 0) {
          return got;
        }
        got.append(buf.data(), static_cast<std::size_t>(n));
      }
    }

    [[nodiscard]] bool sawEof() const noexcept { return sawEof_; }

    void close() noexcept { fd_.reset(); }

   private:
    UniqueFd fd_;
    bool sawEof_{};
  };
}  // namespace

TEST(Server, InvalidBindAddressThrows) {
  Server::Config cfg{testConfig()};
  cfg.bindAddr = "not-an-ip";

  EXPECT_THROW(Server{cfg}, std::invalid_argument);
}

TEST(Server, PortInUseThrows) {
  const Server::Config cfg{testConfig()};
  const Server first{cfg};

  EXPECT_THROW(Server{cfg}, std::system_error);
}

TEST(Server, ListensOnConstruction) {
  const Server::Config cfg{testConfig()};
  const Server server{cfg};

  EXPECT_NO_THROW(Client{cfg.port});
  EXPECT_EQ(server.connectionCount(), 0U);
}

TEST(Server, StopEndsRun) {
  Server server{testConfig()};

  runFor(server);

  SUCCEED();
}

TEST(Server, RunIsRestartableAfterStop) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  runFor(server);

  Client c{cfg.port};
  runFor(server);

  EXPECT_EQ(server.connectionCount(), 1U);
}

TEST(Server, AcceptsClient) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client c{cfg.port};

  runFor(server);

  EXPECT_EQ(server.connectionCount(), 1U);
}

TEST(Server, AcceptsEveryPendingClient) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  std::vector<Client> clients;
  clients.reserve(5);
  for (int i{}; i < 5; ++i) {
    clients.emplace_back(cfg.port);
  }

  runFor(server);

  EXPECT_EQ(server.connectionCount(), 5U);
}

TEST(Server, RepliesToCommand) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client c{cfg.port};

  c.send("*1\r\n$4\r\nPING\r\n");
  runFor(server);

  EXPECT_EQ(c.recvAll(), "+PONG\r\n");
}

TEST(Server, RepliesToPipelinedCommandsInOrder) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client c{cfg.port};

  c.send("SET k v\r\nGET k\r\nDEL k\r\nGET k\r\n");
  runFor(server);

  EXPECT_EQ(c.recvAll(), "+OK\r\n$1\r\nv\r\n:1\r\n$-1\r\n");
}

TEST(Server, ClientsShareOneStore) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client writer{cfg.port};
  Client reader{cfg.port};

  writer.send("SET k v\r\n");
  runFor(server);
  reader.send("GET k\r\n");
  runFor(server);

  EXPECT_EQ(writer.recvAll(), "+OK\r\n");
  EXPECT_EQ(reader.recvAll(), "$1\r\nv\r\n");
}

TEST(Server, PeerCloseRemovesConnection) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client stays{cfg.port};
  Client leaves{cfg.port};
  runFor(server);
  ASSERT_EQ(server.connectionCount(), 2U);

  leaves.close();
  runFor(server);

  EXPECT_EQ(server.connectionCount(), 1U);
}

TEST(Server, QuitRepliesThenCloses) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client c{cfg.port};

  c.send("QUIT\r\n");
  runFor(server);

  EXPECT_EQ(c.recvAll(), "+OK\r\n");
  EXPECT_TRUE(c.sawEof());
  EXPECT_EQ(server.connectionCount(), 0U);
}

TEST(Server, ProtocolErrorRepliesThenCloses) {
  const Server::Config cfg{testConfig()};
  Server server{cfg};
  Client c{cfg.port};

  c.send("*1\n$4\r\nPING\r\n");
  runFor(server);

  const std::string reply{c.recvAll()};
  ASSERT_FALSE(reply.empty());
  EXPECT_EQ(reply.front(), '-');
  EXPECT_TRUE(c.sawEof());
  EXPECT_EQ(server.connectionCount(), 0U);
}

TEST(Server, IdleServerReapsExpiredKeys) {
  Server server{testConfig()};
  const std::int64_t now{tinyredis::nowMs()};
  ASSERT_TRUE(server.store().set("k", "v", now + 5, now));

  // No client ever reads k, so only the active expire cycle can reclaim it.
  runFor(server, 100);

  EXPECT_EQ(server.store().size(), 0U);
  EXPECT_EQ(server.store().expiredKeys(), 1U);
}

TEST(Server, DestructionClosesLiveClients) {
  const Server::Config cfg{testConfig()};
  std::optional<Server> server{std::in_place, cfg};
  Client c{cfg.port};
  runFor(*server);
  ASSERT_EQ(server->connectionCount(), 1U);

  server.reset();

  EXPECT_EQ(c.recvAll(), "");
  EXPECT_TRUE(c.sawEof());
}
