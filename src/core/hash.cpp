#include "core/hash.h"

#include <bit>
#include <cstring>

#include "core/bits.h"

namespace {
  // SipHash reads the message as little-endian words.
  std::uint64_t load64le(const char* p) noexcept {
    std::uint64_t w{};
    std::memcpy(&w, p, sizeof w);  // unaligned-safe; compiles to one load
    if constexpr (std::endian::native == std::endian::big) {
      w = __builtin_bswap64(w);  // std::byteswap is C++23
    }
    return w;
  }

  struct SipState {
    std::uint64_t v0, v1, v2, v3;

    void round() noexcept {
      v0 += v1;
      v1 = std::rotl(v1, 13);
      v1 ^= v0;
      v0 = std::rotl(v0, 32);
      v2 += v3;
      v3 = std::rotl(v3, 16);
      v3 ^= v2;
      v0 += v3;
      v3 = std::rotl(v3, 21);
      v3 ^= v0;
      v2 += v1;
      v1 = std::rotl(v1, 17);
      v1 ^= v2;
      v2 = std::rotl(v2, 32);
    }

    void compress(std::uint64_t m) noexcept {
      v3 ^= m;
      round();  // c = 1
      v0 ^= m;
    }
  };
}  // namespace

namespace tinyredis {

  std::uint64_t sipHash13(const char* p, std::size_t len, std::uint64_t k0,
                          std::uint64_t k1) noexcept {
    // "somepseudorandomlygeneratedbytes"
    SipState s{.v0 = k0 ^ 0x736f6d6570736575ULL,
               .v1 = k1 ^ 0x646f72616e646f6dULL,
               .v2 = k0 ^ 0x6c7967656e657261ULL,
               .v3 = k1 ^ 0x7465646279746573ULL};

    const char* const end{p + (len & ~std::size_t{7})};
    for (; p != end; p += 8) {
      s.compress(load64le(p));
    }

    // Final block: 0-7 tail bytes, len mod 256 in the top byte. The length byte keeps
    // "ab" and "ab\0" apart even though both zero-pad to the same word.
    std::uint64_t b{static_cast<std::uint64_t>(len) << 56};
    switch (len & 7) {
      case 7:
        b |= std::uint64_t{static_cast<unsigned char>(p[6])} << 48;
        [[fallthrough]];
      case 6:
        b |= std::uint64_t{static_cast<unsigned char>(p[5])} << 40;
        [[fallthrough]];
      case 5:
        b |= std::uint64_t{static_cast<unsigned char>(p[4])} << 32;
        [[fallthrough]];
      case 4:
        b |= std::uint64_t{static_cast<unsigned char>(p[3])} << 24;
        [[fallthrough]];
      case 3:
        b |= std::uint64_t{static_cast<unsigned char>(p[2])} << 16;
        [[fallthrough]];
      case 2:
        b |= std::uint64_t{static_cast<unsigned char>(p[1])} << 8;
        [[fallthrough]];
      case 1:
        b |= std::uint64_t{static_cast<unsigned char>(p[0])};
        break;
      default:
        break;
    }
    s.compress(b);

    s.v2 ^= 0xff;
    s.round();  // d = 3
    s.round();
    s.round();
    return s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
  }

  std::uint64_t hashBytes(const char* p, std::size_t len, std::uint64_t seed) noexcept {
    // Stretch the 64-bit seed to SipHash's 128-bit key. Independent of p, so it overlaps
    // with the message loads instead of adding to the dependency chain.
    std::uint64_t state{seed};
    const std::uint64_t k0{splitmix64(state)};
    const std::uint64_t k1{splitmix64(state)};
    return sipHash13(p, len, k0, k1);
  }

}  // namespace tinyredis
