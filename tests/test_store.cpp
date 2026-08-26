#include <gtest/gtest.h>

#include "core/store.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Seed Store::Config with a fixed value so bucket layout is deterministic across runs.
// Each body is a deliberate failure so an unwritten test can never read as green.

TEST(Store, SetThenFind) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, MissingKeyReturnsNull) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, OverwriteReplacesValueWithoutGrowingSize) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, ShrinkingOverwriteKeepsCorrectLength) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, KeysAndValuesAreBinarySafe) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, SpillsValuesLargerThanTheInlineBuffer) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, EraseRemovesTheKey) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, GrowsAndKeepsEveryKeyReachable) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, ClearEmptiesTheTable) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Store, RefusesToExceedMaxKeys) { FAIL() << "TODO(robbie): not written yet"; }
