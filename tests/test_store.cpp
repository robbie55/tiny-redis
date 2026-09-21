#include <gtest/gtest.h>

#include "core/store.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Seed Store::Config with a fixed value so bucket layout is the same every run.
// Unwritten bodies GTEST_SKIP, and ctest counts a skip as a pass: check the skip count.

TEST(Store, SetThenFind) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, MissingKeyReturnsNull) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, OverwriteReplacesValueWithoutGrowingSize) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Store, ShrinkingOverwriteKeepsCorrectLength) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Store, KeysAndValuesAreBinarySafe) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, SpillsValuesLargerThanTheInlineBuffer) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Store, EraseRemovesTheKey) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, GrowsAndKeepsEveryKeyReachable) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, ClearEmptiesTheTable) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, RefusesToExceedMaxKeys) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, EmptyKeyAndEmptyValueAreLegal) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, PreallocatedTableNeverGrows) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, MaxKeysZeroMeansPoolCapacity) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, FillsTheWholePool) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Store, ZeroSeedStillWorks) { GTEST_SKIP() << "TODO(robbie): not written yet"; }
