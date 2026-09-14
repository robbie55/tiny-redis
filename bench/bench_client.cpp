// Pipelined benchmark client.
//
// redis-benchmark produces the headline figure as a third-party measurement. This client
// covers what redis-benchmark can't vary, with a methodology we control.
//
// TODO(robbie): port the harness discipline from pool-allocator.
//   - Warmup before the timed region.
//   - N trials, report the median, not the mean. Reject outliers.
//   - Latency p50 and p99, never an average.
//   - DCE guards so nothing measured gets optimized away.
//   - Pipeline depth as a parameter, for the on/off comparison.
//   - Record machine, compiler, flags and core pinning with every number.
//
// A number that isn't in a committed file under bench/results/ didn't happen.

#include <cstdio>

int main() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  std::fprintf(stderr, "bench client: not written yet\n");
  return 1;
}
