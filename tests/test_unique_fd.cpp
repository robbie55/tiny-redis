#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <system_error>
#include <utility>

#include "net/unique_fd.h"

using tinyredis::UniqueFd;

namespace {
  int openFd() {
    std::array<int, 2> fds{};
    if (::pipe2(fds.data(), O_CLOEXEC) == -1) {
      throw std::system_error(errno, std::generic_category(), "pipe2");
    }
    ::close(fds[1]);
    return fds[0];
  }

  bool isOpen(int fd) {
    struct stat st{};
    return ::fstat(fd, &st) == 0;
  }
}  // namespace

TEST(UniqueFd, DestructorCloses) {
  const int raw{openFd()};
  {
    const UniqueFd fd{raw};
  }

  EXPECT_FALSE(isOpen(raw));
}

TEST(UniqueFd, MovedFromClosesNothing) {
  const int raw{openFd()};
  UniqueFd dst{-1};
  {
    UniqueFd src{raw};
    dst = UniqueFd{std::move(src)};
  }

  EXPECT_TRUE(isOpen(raw));
  EXPECT_EQ(dst.get(), raw);
}

TEST(UniqueFd, MoveAssignClosesPrevious) {
  const int old{openFd()};
  const int raw{openFd()};
  UniqueFd dst{old};

  dst = UniqueFd{raw};

  EXPECT_FALSE(isOpen(old));
  EXPECT_EQ(dst.get(), raw);
}

TEST(UniqueFd, ResetClosesPrevious) {
  const int old{openFd()};
  const int raw{openFd()};
  UniqueFd fd{old};

  fd.reset(raw);

  EXPECT_FALSE(isOpen(old));
  EXPECT_EQ(fd.get(), raw);
}

TEST(UniqueFd, ResetToSameFdKeepsItOpen) {
  const int raw{openFd()};
  UniqueFd fd{raw};

  fd.reset(raw);

  EXPECT_TRUE(isOpen(raw));
  EXPECT_EQ(fd.get(), raw);
}

TEST(UniqueFd, ReleaseLeavesFdOpen) {
  UniqueFd fd{openFd()};

  const int raw{fd.release()};

  EXPECT_FALSE(fd);
  EXPECT_TRUE(isOpen(raw));
  ::close(raw);
}
