#include "net/event_loop.h"

#include "core/todo.h"

namespace tinyredis {

  EventLoop::EventLoop() { unimplemented("EventLoop::EventLoop"); }

  EventLoop::~EventLoop() = default;

  void EventLoop::add(IoHandler& /*h*/, std::uint32_t /*events*/) {
    unimplemented("EventLoop::add");
  }

  void EventLoop::modify(IoHandler& /*h*/, std::uint32_t /*events*/) {
    unimplemented("EventLoop::modify");
  }

  void EventLoop::remove(IoHandler& /*h*/) noexcept { unimplemented("EventLoop::remove"); }

  void EventLoop::run(int /*timeoutMs*/, const std::function<void()>& /*onTick*/) {
    unimplemented("EventLoop::run");
  }

  void EventLoop::stop() noexcept { unimplemented("EventLoop::stop"); }

  std::uint64_t EventLoop::tick() const noexcept { unimplemented("EventLoop::tick"); }

}  // namespace tinyredis
