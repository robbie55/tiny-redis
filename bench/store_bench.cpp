// In-process Store benchmarks (ticket 009): no network, so these isolate policy quality
// and sweep cost from the event loop.
//
//   evict   True-LRU oracle beside the Store. Per eviction, the victim's recency percentile
//           (0% = LRU). Sweep lruSamples {5,10,16} x sizes {1k,16k,256k} x 30 seeds x
//           {uniform, Zipf(0.99)}. Report median/mean/stddev and bad-eviction rate
//           (victim in the most-recent 20%). Bar: <= 20%.
//   expire  Full-sweep time (cursor 0 -> 0) and steady-state stale share across table
//           sizes and expiry mixes, via expiredKeys() and ExpireCycleResult.
//
// TODO(robbie): same harness discipline as bench_client; record seed, K and build flags
// with every row. Output goes to bench/results/.

#include <cstdio>
#include <string_view>

namespace {

  int runEvict() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::fprintf(stderr, "store bench evict: not written yet\n");
    return 1;
  }

  int runExpire() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::fprintf(stderr, "store bench expire: not written yet\n");
    return 1;
  }

}  // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "";
  if (mode == "evict") return runEvict();
  if (mode == "expire") return runExpire();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  std::fprintf(stderr, "usage: tinyredis_store_bench evict|expire\n");
  return 2;
}
