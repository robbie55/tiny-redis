#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace tinyredis {

  // Anything the loop can wake up on, meaning the listening socket and every client socket.
  class IoHandler {
   public:
    IoHandler(const IoHandler&) = delete;
    IoHandler& operator=(const IoHandler&) = delete;
    IoHandler(IoHandler&&) = delete;
    IoHandler& operator=(IoHandler&&) = delete;
    virtual ~IoHandler() = default;

    virtual void onReadable() = 0;
    virtual void onWritable() = 0;

    [[nodiscard]] int fd() const noexcept { return fd_; }

   protected:
    explicit IoHandler(int fd) noexcept : fd_(fd) {}
    int fd_;
  };

  // The epoll loop. One thread owns every socket and the whole store, which is why
  // nothing in core/ needs a lock.
  //
  // TODO(robbie): level-triggered or edge-triggered? Everything else follows from this.
  //   - Level-triggered: epoll keeps reporting a fd while data remains. Forgiving.
  //   - Edge-triggered (EPOLLET): reported once per readiness transition. Every handler
  //     must drain its fd until read() returns EAGAIN, or it hangs holding unread bytes.
  // Work out what each costs in epoll_wait round trips per pipelined batch, pick one, and
  // write the choice down. The connection code has to agree with it.
  //
  // TODO(robbie): sockets must be non-blocking. Which syscalls set that, and why does
  // accept4() exist when accept() plus fcntl() does the same job?
  class EventLoop {
   public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void add(IoHandler& h, std::uint32_t events);
    void modify(IoHandler& h, std::uint32_t events);
    void remove(IoHandler& h) noexcept;

    // Runs until stop(). `onTick` fires after each batch of events and on every timeout.
    // TODO(robbie): this drives the periodic TTL sweep. What does that mean for the
    // epoll_wait timeout when there's no traffic at all?
    void run(int timeoutMs, const std::function<void()>& onTick);

    void stop() noexcept;

    // Incremented once per epoll_wait return.
    // TODO(robbie): Server uses this to read the clock once per iteration instead of once
    // per command. Why does that matter when one wakeup carries hundreds of commands?
    [[nodiscard]] std::uint64_t tick() const noexcept;

   private:
    // TODO(robbie): the epoll fd, a running flag, the tick counter, and the event array
    // for epoll_wait. How big should that array be, and what happens when more fds are
    // ready than it holds?
  };

}  // namespace tinyredis
