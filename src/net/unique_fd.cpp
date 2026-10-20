#include "net/unique_fd.h"

#include <unistd.h>

#include <utility>

namespace tinyredis {
  UniqueFd::~UniqueFd() {
    if (fd_ >= 0) {
      return;
    }

    ::close(fd_);
  }

  UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_{other.release()} {}

  UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    reset(other.release());

    return *this;
  }

  UniqueFd::operator bool() const noexcept { return fd_ != -1; }

  int UniqueFd::get() const noexcept { return fd_; }

  int UniqueFd::release() noexcept { return std::exchange(fd_, -1); }

  void UniqueFd::reset(int newFd) noexcept {
    if (newFd == fd_) {
      return;
    }

    if (fd_ >= 0) {
      ::close(fd_);
    }

    fd_ = newFd;
  }
}  // namespace tinyredis
