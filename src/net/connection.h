#pragma once

#include "core/buffer.h"
#include "net/event_loop.h"

namespace tinyredis {

  class Server;

  // One client socket, with an input buffer fed by read(2) and an output buffer drained
  // by write(2).
  //
  // TODO(robbie): pipelined throughput is won or lost in this class.
  //   - On readable, read until EAGAIN, then parse and execute every complete command in
  //     the input buffer, not only the first.
  //   - Replies accumulate in the output buffer. How many write() calls should a batch of
  //     100 pipelined commands cost, against one write() per command?
  //   - write() can accept fewer bytes than you gave it, or return EAGAIN with none. What
  //     happens to the remainder, and how does the loop know to come back? That's what
  //     onWritable() and EPOLLOUT are for. Arming EPOLLOUT unconditionally costs a wakeup
  //     on every idle connection, so when should it be armed?
  //   - A client can vanish mid-write. What does write() to a closed socket do to the
  //     process by default, and what are the two ways to stop it?
  class Connection final : public IoHandler {
   public:
    Connection(int fd, Server& server);
    ~Connection() override;

    void onReadable() override;
    void onWritable() override;

    [[nodiscard]] bool closed() const noexcept;

   private:
    void processInput();
    void flushOutput();
    void markClosed();
    void updateInterest();

    [[maybe_unused]] Server& server_;
    Buffer in_;
    Buffer out_;
    // TODO(robbie): what else does a connection track? Is it closed, does it still owe
    // bytes to the socket, and should it close once those drain?
  };

}  // namespace tinyredis
