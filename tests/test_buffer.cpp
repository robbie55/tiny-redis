#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "core/buffer.h"

namespace {
  std::string generatePattern(std::size_t size) {
    std::string byte;
    byte.resize(size);

    for (std::size_t i{}; i < size; ++i) {
      byte[i] = static_cast<char>(i % 251);
    }

    return byte;
  }

  using namespace std::literals::string_view_literals;
  constexpr std::size_t kInitialSize{100};

  TEST(Buffer, AppendAndRead) {
    tinyredis::Buffer buf{kInitialSize};
    constexpr std::string kRead{"Hello World"};
    buf.append(kRead);

    auto readable{buf.readable()};

    EXPECT_TRUE(readable == kRead);
    EXPECT_TRUE(buf.size() == kRead.size());
    EXPECT_FALSE(buf.empty());
  }

  TEST(Buffer, ConsumePartialThenRest) {
    tinyredis::Buffer buf{kInitialSize};
    constexpr std::string kRead{"Hello World"};
    buf.append(kRead);

    constexpr std::size_t kPrefix{7};
    buf.consume(kPrefix);

    EXPECT_TRUE(buf.readable() == kRead.substr(kPrefix));

    buf.consume(kRead.size() - kPrefix);

    EXPECT_TRUE(buf.empty());
    EXPECT_TRUE(buf.writableBytes() == buf.capacity());
  }

  TEST(Buffer, CompactsInsteadOfGrowingWhenPrefixWasConsumed) {
    tinyredis::Buffer buf{kInitialSize};

    const std::string kWrite{generatePattern(kInitialSize - 30)};
    buf.append(kWrite);

    // read cursor at 20, 30 bytes in tail, so enough bytes to kWrite 50 if we shifted
    constexpr std::size_t kRead{20};
    buf.consume(kRead);

    buf.ensureWritable(50);
    EXPECT_TRUE(buf.capacity() == kInitialSize);
    EXPECT_TRUE(buf.readable() == kWrite.substr(kRead));
  }

  TEST(Buffer, GrowsAndPreservesPendingBytes) {
    tinyredis::Buffer buf{kInitialSize};

    // grow kWrite ptr, but leave readptr at 0, will force a resize
    const std::string kWrite{generatePattern(kInitialSize - 10)};
    buf.append(kWrite);

    buf.ensureWritable(30);

    EXPECT_TRUE(buf.capacity() == static_cast<std::size_t>(2 * kInitialSize));
    EXPECT_TRUE(buf.readable() == kWrite);
  }

  TEST(Buffer, IsBinarySafe) {
    // payload with max val xFF, and a NUL x00
    tinyredis::Buffer buf{kInitialSize};
    constexpr auto kBinary{
        "\xff\x00"
        "hello"
        "\x00"
        "world"
        "\x7f\x80"sv};
    buf.append(kBinary);

    EXPECT_TRUE(buf.size() == kBinary.size());
    EXPECT_TRUE(buf.readable() == kBinary);
  }

}  // namespace
