#include <gtest/gtest.h>

#include "core/buffer.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// Binary safety matters here: NUL is a legal byte in a Redis key or value.
// Each body is a deliberate failure so an unwritten test can never read as green.

TEST(Buffer, AppendAndRead) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Buffer, ConsumePartialThenRest) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Buffer, CompactsInsteadOfGrowingWhenPrefixWasConsumed) {
  FAIL() << "TODO(robbie): not written yet";
}

TEST(Buffer, GrowsAndPreservesPendingBytes) { FAIL() << "TODO(robbie): not written yet"; }

TEST(Buffer, IsBinarySafe) { FAIL() << "TODO(robbie): not written yet"; }
