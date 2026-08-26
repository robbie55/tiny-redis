#pragma once

#include "core/buffer.h"
#include "net/event_loop.h"

namespace tinyredis {

  class Server;

  // One client socket: an input buffer fed by read(2), an output buffer drained by
  // write(2).
  //
  // TODO(robbie): this class is where the pipelined throughput number is won or lost.
  //   - On readable: read until EAGAIN, then parse and execute EVERY complete command
  //     sitting in the input buffer, not just the first one.
  //   - Replies accumulate in the output buffer. How many write() calls should a batch of
  //     100 pipelined commands cost? Compare that to one write() per command.
  //   - write() is allowed to accept fewer bytes than you gave it, or return EAGAIN with
  //     none. What do you do with the remainder, and how does the loop know to come back?
  //     That is what onWritable() and EPOLLOUT are for -- but arming EPOLLOUT
  //     unconditionally costs you a wakeup on every idle connection. When should it be
  //     armed?
  //   - A client can vanish mid-write. What does write() to a closed socket do to the
  //     process by default, and what are the two ways to stop that?
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

    Server& server_;
    Buffer in_;
    Buffer out_;
    // TODO(robbie): what else does a connection need to track? Consider: is it closed,
    // does it still owe bytes to the socket, and should it close once those are gone?
  };

}  // namespace tinyredis
