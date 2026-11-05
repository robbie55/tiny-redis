#include "net/connection.h"

#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <string_view>
#include <system_error>

#include "core/command.h"
#include "core/resp.h"
#include "net/server.h"
#include "net/unique_fd.h"

namespace tinyredis {

  Connection::Connection(UniqueFd fd, Server& server)
      : IoHandler{std::move(fd)},
        server_{server},
        state_{ConnectionState::kOpen},
        interest_{EPOLLIN} {
    server_.loop().add(*this, interest_);
  }

  Connection::~Connection() { server_.loop().remove(*this); }

  void Connection::onReadable() {
    if (closed()) {
      return;
    }

    in_.ensureWritable(Buffer::kDefaultCapacity);
    const ssize_t ready{::read(fd_.get(), in_.writePtr(), in_.writableBytes())};

    // level triggered with epoll replaces need for draining until EAGAIN
    if (ready > 0) {
      in_.commit(static_cast<std::size_t>(ready));
      processInput();
      flushOutput();

      return;
    }

    if (ready == 0) {  // peer EOF
      markClosed();
      return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return;
    }

    markClosed();
  }

  void Connection::onWritable() {
    if (closed()) {
      return;
    }

    flushOutput();
  }

  bool Connection::closed() const noexcept { return state_ == ConnectionState::kClosed; }

  void Connection::processInput() {
    if (state_ != ConnectionState::kOpen) {
      return;
    }

    const std::int64_t now{server_.now()};

    Command cmd{};
    while (!in_.empty()) {
      const ParseResult parseRes{parseCommand(in_.readable(), cmd)};

      if (parseRes.status == ParseStatus::kIncomplete) {
        return;
      }

      if (parseRes.status == ParseStatus::kProtocolError) {
        reply::error(out_, parseRes.error);
        state_ = ConnectionState::kClosing;
        return;
      }

      const DispatchResult dispatchRes{dispatch(server_.store(), cmd, now, out_)};
      in_.consume(parseRes.consumed);

      if (dispatchRes == DispatchResult::kCloseConnection) {
        state_ = ConnectionState::kClosing;
        return;
      }
    }
  }

  void Connection::flushOutput() {
    while (!out_.empty()) {
      const ssize_t written{::write(fd_.get(), out_.readable().data(), out_.readable().size())};
      if (written > 0) {
        out_.consume(static_cast<std::size_t>(written));
        continue;
      }

      if (written == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      if (errno == EINTR) {
        continue;
      }

      markClosed();
      return;
    }

    if (out_.empty() && state_ == ConnectionState::kClosing) {
      markClosed();
      return;
    }

    updateInterest();
  }

  void Connection::markClosed() {
    if (closed()) {
      return;
    }

    state_ = ConnectionState::kClosed;
    server_.retire(*this);
  }

  void Connection::updateInterest() {
    std::uint32_t wanted{};
    if (!out_.empty()) {
      wanted |= EPOLLOUT;
    }
    if (state_ == ConnectionState::kOpen) {
      wanted |= EPOLLIN;
    }

    if (wanted == interest_) {
      return;
    }

    try {
      server_.loop().modify(*this, wanted);
    } catch (const std::system_error& e) {
      assert(e.code().value() != ENOENT && e.code().value() != EBADF);

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): fprintf is variadic, no alternative
      std::fprintf(stderr, "Connection::updateInterest -> %s\n", e.what());
      markClosed();
      return;
    }

    interest_ = wanted;
  }

}  // namespace tinyredis
