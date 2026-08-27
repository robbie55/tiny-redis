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
  const auto kInitialSize{100};

  TEST(Buffer, AppendAndRead) {
    tinyredis::Buffer buf{kInitialSize};
    const std::string read{"Hello World"};
    buf.append(read);

    auto readable{buf.readable()};

    EXPECT_TRUE(readable == read);
    EXPECT_TRUE(buf.size() == read.size());
    EXPECT_FALSE(buf.empty());
  }

  TEST(Buffer, ConsumePartialThenRest) {
    tinyredis::Buffer buf{kInitialSize};
    const std::string read{"Hello World"};
    buf.append(read);

    const auto prefix{7};
    buf.consume(prefix);

    EXPECT_TRUE(buf.readable() == read.substr(prefix));

    buf.consume(read.size() - prefix);

    EXPECT_TRUE(buf.empty());
    EXPECT_TRUE(buf.writableBytes() == buf.capacity());
  }

  TEST(Buffer, CompactsInsteadOfGrowingWhenPrefixWasConsumed) {
    tinyredis::Buffer buf{kInitialSize};

    const std::string write{generatePattern(kInitialSize - 30)};
    buf.append(write);

    // read cursor at 20, 30 bytes in tail, so enough bytes to write 50 if we shifted
    const auto read{20};
    buf.consume(read);

    buf.ensureWritable(50);
    EXPECT_TRUE(buf.capacity() == kInitialSize);
    EXPECT_TRUE(buf.readable() == write.substr(read));
  }

  TEST(Buffer, GrowsAndPreservesPendingBytes) {
    tinyredis::Buffer buf{kInitialSize};

    // grow write ptr, but leave readptr at 0, will force a resize
    const std::string write{generatePattern(kInitialSize - 10)};
    buf.append(write);

    buf.ensureWritable(30);

    EXPECT_TRUE(buf.capacity() == static_cast<std::size_t>(2 * kInitialSize));
    EXPECT_TRUE(buf.readable() == write);
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
