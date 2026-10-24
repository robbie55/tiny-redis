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

    explicit Server(Config cfg);
    ~Server() = default;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void run();
    void stop() noexcept;

    Store& store() noexcept;
    EventLoop& loop() noexcept;

    std::int64_t now() noexcept;

    void retire(Connection& c);

    [[nodiscard]] std::size_t connectionCount() const noexcept;

   private:
    // The listening socket's handler.
    class Acceptor final : public IoHandler {
     public:
      Acceptor(const Config& cfg, Server& server);
      ~Acceptor() override;

      Acceptor(const Acceptor&) = delete;
      Acceptor& operator=(const Acceptor&) = delete;
      Acceptor(Acceptor&&) = delete;
      Acceptor& operator=(Acceptor&&) = delete;

      void onReadable() override;
      void onWritable() override;

     private:
      Server& server_;
    };

    void acceptPending();
    void reapRetired();

    Config cfg_;
    Store store_;
    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::vector<std::unique_ptr<Connection>> retired_;

    std::int64_t cachedNow_;
    std::int64_t lastSweepMs_;
    std::uint64_t cachedTick_;
  };

}  // namespace tinyredis
