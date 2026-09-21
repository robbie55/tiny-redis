#include <gtest/gtest.h>

#include "core/store.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Pass `now` explicitly instead of sleeping, so these stay deterministic under ASan.
// Unwritten bodies GTEST_SKIP, and ctest counts a skip as a pass: check the skip count.

TEST(Ttl, KeyIsVisibleBeforeItsDeadline) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, KeyIsGoneAfterItsDeadline) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ReportsRemainingMilliseconds) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ReportsMinusOneForPersistentKeyAndMinusTwoForMissing) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Ttl, SetExpireAtOnlySucceedsForLiveKeys) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, PlainSetClearsAnExistingDeadline) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, OverwritingAnExpiredKeyStartsFresh) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ActiveExpireCycleReapsWithoutAnyAccess) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Ttl, ActiveExpireCycleLeavesLiveKeysAlone) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, KeyIsLiveAtItsDeadline) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, PastDeadlineSetDeletesTheKey) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, KeepTtlPreservesTheDeadline) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, LazyExpiryCountsInExpiredKeys) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ActiveExpireCycleStopsAtMaxPasses) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ActiveExpireCycleStopsRepeatingAtTenPercent) {
  GTEST_SKIP() << "TODO(robbie): not written yet";
}

TEST(Ttl, TableNeverShrinksAfterMassExpiry) { GTEST_SKIP() << "TODO(robbie): not written yet"; }

TEST(Ttl, ClearRestartsTheSweep) { GTEST_SKIP() << "TODO(robbie): not written yet"; }
