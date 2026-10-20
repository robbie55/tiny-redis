#include "net/event_loop.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <system_error>

namespace {
  epoll_event buildEpollEvent(tinyredis::IoHandler& h, std::uint32_t events) {
    epoll_event event{};
    event.events = events;
    event.data.ptr = &h;

    return event;
  }
}  // namespace

namespace tinyredis {

  EventLoop::EventLoop() {
    const int epfd{epoll_create1(EPOLL_CLOEXEC)};

    if (epfd == -1) {
      throw std::system_error(errno, std::generic_category(),
                              "EventLoop::EventLoop -> Failed to create an epfd");
    }

    epollFd_ = epfd;
  }

  EventLoop::~EventLoop() { close(epollFd_); }

  // epollCtl/remove intentionally non-const: they mutate kernel epoll state via epollFd_.
  // NOLINTNEXTLINE(readability-make-member-function-const)
  void EventLoop::epollCtl(IoHandler& h, int flag, epoll_event event, const char* msg) {
    const int ctl{epoll_ctl(epollFd_, flag, h.fd(), &event)};

    if (ctl == -1) {
      throw std::system_error(errno, std::generic_category(), msg);
    }
  }

  void EventLoop::add(IoHandler& h, std::uint32_t events) {
    epollCtl(h, EPOLL_CTL_ADD, buildEpollEvent(h, events),
             "EventLoop::add -> Failed to add with epoll_ctl");
  }

  void EventLoop::modify(IoHandler& h, std::uint32_t events) {
    epollCtl(h, EPOLL_CTL_MOD, buildEpollEvent(h, events),
             "EventLoop::modify -> Failed to modify with epoll_ctl");
  }

  // NOLINTNEXTLINE(readability-make-member-function-const)
  void EventLoop::remove(IoHandler& h) noexcept {
    [[maybe_unused]] const int ctl{epoll_ctl(epollFd_, EPOLL_CTL_DEL, h.fd(), nullptr)};

    assert(ctl != -1 && "EventLoop::remove -> assert caught ctl == -1");
  }

  void EventLoop::run(int timeoutMs, const std::function<void()>& onTick) {
    running_ = true;

    while (running_) {
      const int eventsReady{
          epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs)};

      // interrupted by a signal; recheck running_
      if (eventsReady == -1 && errno == EINTR) {
        continue;
      }
      if (eventsReady == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "EventLoop::run -> Failed to run epoll_wait");
      }

      tickCount_ += 1;

      // eventsReady ≤ events_.size(), never negative here
      for (std::size_t i{}; i < static_cast<std::size_t>(eventsReady); ++i) {
        epoll_event event{events_[i]};
        IoHandler* io{static_cast<IoHandler*>(event.data.ptr)};

        if ((event.events & (EPOLLIN | EPOLLERR | EPOLLHUP)) != 0) {
          io->onReadable();
        }

        if ((event.events & EPOLLOUT) != 0) {
          io->onWritable();
        }
      }
      onTick();
    }
  }

  void EventLoop::stop() noexcept { running_ = false; }

  std::uint64_t EventLoop::tick() const noexcept { return tickCount_; }

}  // namespace tinyredis
