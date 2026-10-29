#pragma once

#include <cstdint>

#include "core/buffer.h"
#include "net/event_loop.h"
#include "net/unique_fd.h"

namespace tinyredis {

  enum class ConnectionState : std::uint8_t { kOpen, kClosing, kClosed };

  class Server;

  // One client socket, with an input buffer fed by read(2) and an output buffer drained
  // by write(2).
  class Connection final : public IoHandler {
   public:
    Connection(UniqueFd fd, Server& server);
    ~Connection() override;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    void onReadable() override;
    void onWritable() override;

    [[nodiscard]] bool closed() const noexcept;

   private:
    void processInput();
    void flushOutput();
    void markClosed();
    void updateInterest();

    Server& server_;
    Buffer in_;
    Buffer out_;
    ConnectionState state_;
    std::uint32_t interest_;  // mask registered with epoll
  };

}  // namespace tinyredis
