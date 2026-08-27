#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/store.h"
#include "net/connection.h"
#include "net/event_loop.h"

namespace tinyredis {

  // Owns the listening socket, the store, and every live connection.
  class Server {
   public:
    struct Config {
      std::string bindAddr{"127.0.0.1"};
      std::uint16_t port{6380};
      int backlog{512};
      int expireIntervalMs{100};
      std::size_t expireSampleBuckets{20};
      Store::Config store{};
    };

    explicit Server(const Config& cfg);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop() noexcept;

    Store& store() noexcept;
    EventLoop& loop() noexcept;

    // TODO(robbie): one clock read per event-loop iteration, shared by every command in
    // the batch -- not one read per command. Cache it against the loop's tick counter.
    std::int64_t now() noexcept;

    // Called by a Connection that has decided it is finished.
    // TODO(robbie): you cannot just delete it here. A socket can be reported both
    // readable and writable in the same epoll_wait return, and the second callback would
    // then run against a freed object. When is it actually safe to destroy?
    void retire(Connection& c);

    [[nodiscard]] std::size_t connectionCount() const noexcept;

   private:
    // The listening socket's handler.
    class Acceptor final : public IoHandler {
     public:
      Acceptor(int fd, Server& server) noexcept;
      void onReadable() override;
      void onWritable() override;

     private:
      [[maybe_unused]] Server& server_;
    };

    // TODO(robbie): a single accept() per wakeup is a bug under edge-triggered epoll --
    // and a throughput problem under any mode when 50 clients connect at once. Drain it.
    // Also: what does SO_REUSEADDR fix, and what happens without TCP_NODELAY on the
    // accepted socket when you are measuring latency?
    void acceptPending();
    void reapRetired();

    Config cfg_;
    Store store_;
    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    // TODO(robbie): connections pending destruction, the cached clock reading and the
    // tick it belongs to, and when the last expiry sweep ran.
  };

}  // namespace tinyredis
