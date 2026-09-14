#include <gtest/gtest.h>

#include "core/command.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Assert on the exact wire bytes. That catches RESP framing mistakes redis-cli would hide.
// Each body fails on purpose so an unwritten test never reads as green.

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
