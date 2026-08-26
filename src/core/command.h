#pragma once

#include <cstdint>

#include "core/buffer.h"
#include "core/resp.h"
#include "core/store.h"

namespace tinyredis {

  enum class DispatchResult : std::uint8_t { kContinue, kCloseConnection };

  // Executes one parsed command and appends its RESP reply to `out`.
  //
  // The command set (core spec): GET, SET, DEL, EXISTS, EXPIRE, TTL, PING.
  // Worth adding once those work, for redis-cli ergonomics: ECHO, DBSIZE, FLUSHALL, QUIT.
  //
  // TODO(robbie): things to get right, roughly in the order they will bite you.
  //   - Command names are case-insensitive on the wire. "get", "GET" and "GeT" are one
  //     command. Do that without allocating.
  //   - Wrong arity gets a specific error: "-ERR wrong number of arguments for 'get'
  //     command". Match Redis's text; redis-cli surfaces it verbatim.
  //   - SET takes options: EX <s>, PX <ms>, KEEPTTL. What does a bare SET do to an
  //     existing key's TTL, and is that the same thing KEEPTTL does?
  //   - `now` is passed in rather than read here. Every command in one pipelined batch
  //     sees the same timestamp. Convince yourself that is correct before relying on it.
  //   - EXPIRE takes a relative number of seconds, but Store wants an absolute deadline.
  //     Where does that conversion belong, and what does a negative TTL mean?
  //
  // Returns kCloseConnection for QUIT, and for anything the connection cannot recover
  // from.
  DispatchResult dispatch(Store& store, const Command& cmd, std::int64_t now, Buffer& out);

}  // namespace tinyredis
