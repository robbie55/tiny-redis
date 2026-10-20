#pragma once

#include <sys/epoll.h>

#include <array>
#include <cstdint>
#include <functional>

#include "net/unique_fd.h"

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

    [[nodiscard]] int fd() const noexcept { return fd_.get(); }

   protected:
    explicit IoHandler(UniqueFd fd) noexcept : fd_{std::move(fd)} {}
    UniqueFd fd_;
  };

  // The epoll loop. One thread owns every socket and the whole store
  // Level triggered
  class EventLoop {
   public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    void add(IoHandler& h, std::uint32_t events);
    void modify(IoHandler& h, std::uint32_t events);
    void remove(IoHandler& h) noexcept;

    // Runs until stop(). `onTick` fires after each batch of events and on every timeout.
    // timeoutMs bounds the TTL sweep interval on an idle server.
    void run(int timeoutMs, const std::function<void()>& onTick);

    void stop() noexcept;

    // Incremented once per epoll_wait return.
    [[nodiscard]] std::uint64_t tick() const noexcept;

   private:
    std::array<epoll_event, 1024> events_{};
    int epollFd_{-1};
    std::uint64_t tickCount_{};
    bool running_{};

    void epollCtl(IoHandler& h, int flag, epoll_event event, const char* msg);
  };
}  // namespace tinyredis
