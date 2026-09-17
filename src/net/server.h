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

    // TODO(robbie): read the clock once per event-loop iteration and share it across the
    // batch. Cache it against the loop's tick counter.
    std::int64_t now() noexcept;

    // Called by a Connection that is finished.
    // TODO(robbie): you can't delete it here. epoll_wait can report a socket readable and
    // writable in one return, and the second callback would run on a freed object. When
    // is it safe to destroy?
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

    // TODO(robbie): one accept() per wakeup is a bug under edge-triggered epoll, and a
    // throughput problem in any mode when 50 clients connect at once. Drain it. Also, what
    // does SO_REUSEADDR fix, and what does leaving TCP_NODELAY off the accepted socket do
    // to latency measurements?
    void acceptPending();
    void reapRetired();

    Config cfg_;
    Store store_;
    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    // TODO(robbie): connections pending destruction, the cached clock reading and its
    // tick, and when the last expiry sweep ran.
  };

}  // namespace tinyredis
