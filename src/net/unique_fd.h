#pragma once

class UniqueFd {
 public:
  explicit UniqueFd(int fd) noexcept : fd_{fd} {}

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&&) noexcept;
  UniqueFd& operator=(UniqueFd&&) noexcept;

  ~UniqueFd();

  [[nodiscard]] int get() const noexcept;
  [[nodiscard]] int release() noexcept;
  void reset(int newFd = -1);

  explicit operator bool() const noexcept;

 private:
  int fd_{-1};
};
