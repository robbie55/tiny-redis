#include "net/server.h"

#include <arpa/inet.h>
#include <fcntl.h>
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

namespace {
  using tinyredis::Server;
  using tinyredis::UniqueFd;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): open(2) is variadic, no alternative
  UniqueFd openReserve() { return UniqueFd{::open("/dev/null", O_RDONLY | O_CLOEXEC)}; }

  enum class AcceptFailure : std::uint8_t { kDrained, kRetry, kShed, kPause };

  // Retry only errors that consumed the queued connection. One that didn't stays queued,
  // and retrying it spins. Pause is the default because a wrong guess costs one tick.
  AcceptFailure classifyAcceptErrno(int err) noexcept {
    switch (err) {
      case EAGAIN:  // == EWOULDBLOCK on Linux
        return AcceptFailure::kDrained;
      case EINTR:
      case ECONNABORTED:
      case EPERM:
      // accept(2): pending network errors on the new socket, retry like EAGAIN
      case EPROTO:
      case ENOPROTOOPT:
      case ENETDOWN:
      case ENETUNREACH:
      case ENONET:     // NOLINT(misc-include-cleaner): <cerrno> provides it via asm-generic
      case EHOSTDOWN:  // NOLINT(misc-include-cleaner)
      case EHOSTUNREACH:
      case EOPNOTSUPP:
      case ETIMEDOUT:
        return AcceptFailure::kRetry;
      case EMFILE:
      case ENFILE:
        return AcceptFailure::kShed;
      // Listener misuse: a bug, not a runtime condition. Release pauses instead of spinning.
      case EBADF:
      case ENOTSOCK:
      case EINVAL:
      case EFAULT:
        assert(false && "classifyAcceptErrno -> accept4 on a broken listener");
        return AcceptFailure::kPause;
      default:
        return AcceptFailure::kPause;
    }
  }

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

namespace tinyredis {

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
        cachedTick_{std::numeric_limits<std::uint64_t>::max()},
        reserveFd_{openReserve()} {}

  void Server::run() {
    loop_.run(cfg_.expireIntervalMs, [this] {
      reapRetired();
      resumeAccepting();
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

      if (!pendingFd) {
        if (handleAcceptFailure(errno)) {
          continue;
        }
        return;
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

  // Returns whether acceptPending should keep accepting.
  bool Server::handleAcceptFailure(int err) {
    switch (classifyAcceptErrno(err)) {
      case AcceptFailure::kDrained:
        return false;
      case AcceptFailure::kRetry:
        return true;
      case AcceptFailure::kShed:
        if (shedPending()) {
          return true;
        }
        break;
      case AcceptFailure::kPause:
        break;
    }

    pauseAccepting(err);
    return false;
  }

  // Out of fds, accept4 leaves the connection queued and LT re-fires at once: a busy
  // loop. Spend the reserve to accept and close it, so the peer gets a FIN, not a hang.
  bool Server::shedPending() {
    if (!reserveFd_) {
      return false;
    }

    reserveFd_.reset();
    UniqueFd shed{::accept4(acceptor_->fd(), nullptr, nullptr, SOCK_CLOEXEC)};
    // ENFILE is system-wide: another process can take the slot just freed.

    const bool validShed{static_cast<bool>(shed)};
    shed.reset();

    // open reserve after clearing out shed
    reserveFd_ = openReserve();

    return validShed;
  }

  // Stop asking epoll about the listener; peers wait in the backlog until a tick resumes.
  void Server::pauseAccepting(int err) {
    if (acceptPaused_) {
      return;
    }

    loop_.modify(*acceptor_, 0);
    acceptPaused_ = true;
    std::cerr << "Server::acceptPending -> accept paused: " << std::generic_category().message(err)
              << '\n';
  }

  void Server::resumeAccepting() {
    if (!acceptPaused_) {
      return;
    }

    reserveFd_ = openReserve();
    if (!reserveFd_) {
      return;
    }

    loop_.modify(*acceptor_, EPOLLIN);
    acceptPaused_ = false;
  }

}  // namespace tinyredis
