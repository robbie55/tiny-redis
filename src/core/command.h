#pragma once

#include <cstdint>

#include "core/buffer.h"
#include "core/resp.h"
#include "core/store.h"

namespace tinyredis {

  enum class DispatchResult : std::uint8_t { kContinue, kCloseConnection };

  // Executes one parsed command and appends its RESP reply to `out`.
  //
  // Core commands: GET, SET, DEL, EXISTS, EXPIRE, TTL, PING. ECHO, DBSIZE, FLUSHALL and
  // QUIT come after, because redis-cli sessions reach for them.
  //
  // TODO(robbie): roughly in the order they'll bite.
  //   - Command names are case-insensitive, so "get", "GET" and "GeT" match. Do it
  //     without allocating.
  //   - Wrong arity gets "-ERR wrong number of arguments for 'get' command". redis-cli
  //     prints it verbatim, so match Redis's text.
  //   - SET takes EX <s>, PX <ms> or KEEPTTL. What does a bare SET do to an existing
  //     key's TTL, and is that what KEEPTTL does?
  //   - The caller passes `now`, so every command in a pipelined batch sees one
  //     timestamp. Convince yourself that's correct before relying on it.
  //   - EXPIRE takes relative seconds, but Store wants an absolute deadline. Where does
  //     that conversion belong, and what does a negative TTL mean?
  //
  // Returns kCloseConnection for QUIT and for anything the connection can't recover from.
  DispatchResult dispatch(Store& store, const Command& cmd, std::int64_t now, Buffer& out);

}  // namespace tinyredis
