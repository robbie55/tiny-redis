#include <gtest/gtest.h>

#include "core/store.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Seed with a fixed value; assert behavior, never bucket layout.
// Unwritten bodies GTEST_SKIP, and ctest counts a skip as a pass: check the skip count.

TEST(Evict, NoevictionRefusesANewKeyWhenFull) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Evict, NoevictionStillAllowsOverwriteWhenFull) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Evict, AllkeysLruEvictsExactlyOnePerInsert) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Evict, EvictionCountsInEvictions) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Evict, ExpiredCandidateCountsAsAnExpiry) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Evict, FindWithoutTouchLeavesRecencyAlone) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Evict, BadEvictionRateStaysWithinTwentyPercent) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}
