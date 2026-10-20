#include "unique_fd.h"

#include <unistd.h>

UniqueFd::~UniqueFd() {
  if (fd_ < 0) {
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

int UniqueFd::release() noexcept {
  int fd{fd_};
  reset();

  return fd;
};

void UniqueFd::reset(int newFd) {
  ::close(fd_);
  fd_ = newFd;
}
