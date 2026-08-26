#include <gtest/gtest.h>

#include "core/command.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Assert on the exact bytes that would go on the wire -- that is what catches RESP framing mistakes
// redis-cli would hide. Each body is a deliberate failure so an unwritten test can never read as
// green.

TEST(Commands, Ping) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, SetAndGet) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, SetWithExpiry) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, SetWithPxAndKeepTtl) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, SetRejectsBadExpiry) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, DelAndExists) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, ExpireAndTtl) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, NonPositiveExpireDeletesTheKey) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, DbsizeAndFlushall) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, QuitAsksForClose) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Commands, UnknownCommandAndArityErrors) { FAIL() << "TODO(robbie): not written yet"; }
