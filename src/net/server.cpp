#include "net/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "net/event_loop.h"
#include "net/unique_fd.h"

namespace tinyredis {

  namespace {

    [[noreturn]] void throwSys(const char* msg) {
      throw std::system_error(errno, std::generic_category(), msg);
    }

    UniqueFd openListener(const Server::Config& cfg) {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(cfg.port);
      if (inet_pton(AF_INET, cfg.bindAddr.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("Server -> invalid bind address: " + cfg.bindAddr);
      }

      UniqueFd fd{::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)};
      if (fd.get() == -1) {
        throwSys("Server -> failed to create socket");
      }

      const int reuse{1};
      if (::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        throwSys("Server -> failed to set SO_REUSEADDR");
      }
      if (::bind(fd.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        throwSys("Server -> failed to bind");
      }
      if (::listen(fd.get(), cfg.backlog) == -1) {
        throwSys("Server -> failed to listen");
      }

      return fd;
    }

  }  // namespace

  Server::Acceptor::Acceptor(const Config& cfg, Server& server)
      : IoHandler{openListener(cfg)}, server_{server} {
    server_.loop_.add(*this, EPOLLIN);
  }

  Server::Acceptor::~Acceptor() { server_.loop_.remove(*this); }

  void Server::Acceptor::onReadable() { server_.acceptPending(); }

  void Server::Acceptor::onWritable() {}

  Server::Server(Config cfg)
      : cfg_{std::move(cfg)},
        store_{cfg_.store},
        acceptor_{std::make_unique<Acceptor>(cfg_, *this)},
        cachedNow_{},
        lastSweepMs_{},
        cachedTick_{std::numeric_limits<std::uint64_t>::max()} {}

  void Server::run() {
    loop_.run(cfg_.expireIntervalMs, [this] {
      reapRetired();
      const auto tNow{now()};
      if (tNow - lastSweepMs_ >= cfg_.expireIntervalMs) {
        store_.activeExpireCycle(tNow);
        lastSweepMs_ = tNow;
      }
    });
    reapRetired();
  }

  void Server::stop() noexcept { loop_.stop(); }

  Store& Server::store() noexcept { return store_; }

  EventLoop& Server::loop() noexcept { return loop_; }

  std::int64_t Server::now() noexcept {
    const std::uint64_t lt{loop_.tick()};
    if (lt == cachedTick_) {
      return cachedNow_;
    }

    cachedNow_ = nowMs();
    cachedTick_ = lt;

    return cachedNow_;
  }

  void Server::retire(Connection& c) {
    auto toRetire{connections_.find(c.fd())};

    assert(toRetire != connections_.end() && "Server::retire -> connection not found in map");
    if (toRetire == connections_.end()) {
      return;
    }

    retired_.push_back(std::move(toRetire->second));
    connections_.erase(toRetire);
  }

  std::size_t Server::connectionCount() const noexcept { return connections_.size(); }

  void Server::acceptPending() {
    for (;;) {
      UniqueFd pendingFd{
          ::accept4(acceptor_->fd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC)};

      if (!pendingFd && (errno == ECONNABORTED || errno == EINTR)) {
        continue;
      }

      if (!pendingFd) {
        break;
      }

      const int opt{1};
      if (::setsockopt(pendingFd.get(), IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) {
        continue;
      }

      try {
        auto connection{std::make_unique<Connection>(std::move(pendingFd), *this)};
        connections_.emplace(connection->fd(), std::move(connection));
      } catch (const std::system_error& systemErr) {
        std::cerr << "Server::acceptPending -> " << systemErr.what() << '\n';
      }
    }
  }

  void Server::reapRetired() { retired_.clear(); }

}  // namespace tinyredis
