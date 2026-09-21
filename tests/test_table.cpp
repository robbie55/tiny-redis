#include <gtest/gtest.h>

#include "core/table.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Seed with a fixed value; assert behavior, never bucket layout.
// Unwritten bodies GTEST_SKIP, and ctest counts a skip as a pass: check the skip count.
// Spill-leak checks need LSan, which only runs in the Linux container.

TEST(Table, RoundsInitialBucketsUpToAPowerOfTwo) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Table, InsertAndRemoveMoveUsedByOne) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, RehashKeepsEveryEntryAndLeavesUsedAlone) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Table, AssignCrossesTheInlineSpillBoundaryBothWays) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Table, DestructorFreesSpillBlocks) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, ScanFullCycleVisitsEveryEntryOnce) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, ScanAcrossGrowthMissesNoneAndRepeatsNone) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Table, ScanVisitorMayRemoveTheEntry) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, SampleSomeReturnsDistinctEntries) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, SampleSomeStopsAtTenTimesKBuckets) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Table, SampleSomeOnAnEmptyTableReturnsZero) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}
