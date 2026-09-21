#include <gtest/gtest.h>

#include "core/bits.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Unwritten bodies GTEST_SKIP, and ctest counts a skip as a pass: check the skip count.

TEST(Bits, NextPow2RoundsUpAndKeepsPowersOfTwo) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Bits, NextCursorVisitsEveryBucketOnceThenReturnsZero) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Bits, NextCursorCycleSurvivesADoubling) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

// Reference: seed 0 yields 0xe220a8397b1dcdaf first (Vigna's splitmix64.c).
TEST(Bits, Splitmix64MatchesTheReferenceSequence) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}
