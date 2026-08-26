#include <gtest/gtest.h>

#include "core/store.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Time is a parameter, never a sleep -- pass 'now' explicitly so these stay deterministic under
// ASan. Each body is a deliberate failure so an unwritten test can never read as green.

TEST(Ttl, KeyIsVisibleBeforeItsDeadline) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, KeyIsGoneAtAndAfterItsDeadline) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, ReportsRemainingMilliseconds) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, ReportsMinusOneForPersistentKeyAndMinusTwoForMissing) {
  FAIL() << "TODO(robbie): not written yet";
}

TEST(Ttl, SetExpireAtOnlySucceedsForLiveKeys) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, PlainSetClearsAnExistingDeadline) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, OverwritingAnExpiredKeyStartsFresh) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, ActiveExpireCycleReapsWithoutAnyAccess) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Ttl, ActiveExpireCycleLeavesLiveKeysAlone) { FAIL() << "TODO(robbie): not written yet"; }
