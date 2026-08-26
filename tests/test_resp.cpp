#include <gtest/gtest.h>

#include "core/resp.h"

// TODO(robbie): the test names below are a coverage checklist, not a limit.
// ReportsIncompleteForEveryPrefix is the one that catches real fragmentation bugs -- feed it every
// prefix of a valid command in a loop. Each body is a deliberate failure so an unwritten test can
// never read as green.

TEST(RespParser, ParsesArrayOfBulkStrings) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, ReportsIncompleteForEveryPrefix) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, ConsumesOneCommandAtATimeFromAPipelinedBatch) {
  FAIL() << "TODO(robbie): not written yet";
}

TEST(RespParser, HandlesBinarySafeBulkStrings) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, EmptyBulkStringIsValid) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, AcceptsInlineCommands) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, InlineCommandSplitsOnWhitespace) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, EmptyArrayIsAConsumedNoOp) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, RejectsMalformedInput) { FAIL() << "TODO(robbie): not written yet"; }

TEST(RespParser, RejectsBulkStringNotTerminatedByCrlf) {
  FAIL() << "TODO(robbie): not written yet";
}

TEST(RespReply, EncodesEachType) { FAIL() << "TODO(robbie): not written yet"; }
