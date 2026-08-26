// Pipelined benchmark client.
//
// This is a deliverable, not a nice-to-have: the resume bullet is built on the number
// this produces. redis-benchmark gives you the headline figure and is the credible
// third-party measurement; this client exists so you can vary things redis-benchmark
// will not, and so the methodology is yours.
//
// TODO(robbie): port the harness discipline from your pool-allocator repo.
//   - Warmup before the timed region.
//   - N trials, report the MEDIAN, not the mean. Reject outliers.
//   - Latency percentiles p50 and p99. An average latency number is not defensible.
//   - DCE guards so nothing you measure gets optimized away.
//   - Pipelining depth as a parameter, so you can show the on/off comparison.
//   - Record the machine, compiler, flags, and core pinning alongside every number.
//
// Reporting rule for this project: a number that is not in a committed results file did
// not happen. bench/results/ is gitignored for raw output -- keep the summary you publish
// checked in.

#include <cstdio>

int main() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  std::fprintf(stderr, "bench client: not written yet\n");
  return 1;
}
