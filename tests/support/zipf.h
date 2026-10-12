#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/bits.h"

namespace tinyredis {

  // Zipf(s) over [0, n) by inverse CDF. Own rng so draws match across standard libraries.
  class Zipf {
   public:
    Zipf(std::size_t n, double s, std::uint64_t seed) : cdf_(n), rng_{seed} {
      double sum{0};
      for (std::size_t i{0}; i < n; ++i) {
        sum += 1.0 / std::pow(static_cast<double>(i + 1), s);
        cdf_[i] = sum;
      }
      for (double& c : cdf_) {
        c /= sum;
      }
    }

    std::size_t next() {
      const double u{static_cast<double>(splitmix64(rng_) >> 11) * 0x1.0p-53};
      const auto it{std::ranges::upper_bound(cdf_.begin(), cdf_.end(), u)};
      return std::min(static_cast<std::size_t>(it - cdf_.begin()), cdf_.size() - 1);
    }

   private:
    std::vector<double> cdf_;
    std::uint64_t rng_;
  };

}  // namespace tinyredis
