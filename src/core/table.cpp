#include "core/table.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>

#include "core/bits.h"
#include "core/clock.h"
#include "core/entry.h"
#include "core/hash.h"
#include "core/todo.h"

namespace tinyredis {

  Table::Table(std::size_t initialBuckets, std::uint64_t hashSeed) : hashSeed_{hashSeed} {
    buckets_.assign(std::max(kMinBuckets, nextPow2(initialBuckets)), nullptr);
  }

  Table::~Table() { clear(); }

  Entry* Table::find(std::string_view key) noexcept {
    const std::uint64_t h{hashBytes(key.data(), key.size(), hashSeed_)};
    const auto h32{static_cast<std::uint32_t>(h)};

    for (Entry* e{buckets_[bucketOf(h32)]}; e != nullptr; e = e->next) {
      if (e->hash == h32 && e->keyLen == key.size() && e->key() == key) {
        return e;
      }
    }

    return nullptr;
  }

  Entry* Table::insert(std::string_view key, std::string_view value) {
    const auto keyLen{static_cast<std::uint32_t>(key.size())};
    const auto valLen{static_cast<std::uint32_t>(value.size())};
    const bool spill{keyLen + valLen > kInlineCap};

    // Before taking a block, so a throwing new can't leak it.
    char* heap{spill ? new char[keyLen + valLen] : nullptr};

    Entry* e{pool_.tryAllocate()};
    if (e == nullptr) {
      delete[] heap;
      return nullptr;
    }

    e->hash = static_cast<std::uint32_t>(hashBytes(key.data(), key.size(), hashSeed_));
    e->keyLen = keyLen;
    e->valLen = valLen;
    e->expiresAtMs = kNoExpiry;
    e->lruClock = 0;

    char* dst{spill ? (e->bufPtr = heap) : e->inlineBuf.data()};
    std::memcpy(dst, key.data(), keyLen);
    std::memcpy(dst + keyLen, value.data(), valLen);

    Entry*& head{buckets_[bucketOf(e->hash)]};
    e->next = head;
    head = e;

    ++liveCount_;
    maybeGrow();
    return e;
  }

  void Table::assign(Entry* e, std::string_view value) {
    const auto valLen{static_cast<std::uint32_t>(value.size())};
    const bool wasSpilled{e->spilled()};
    const bool willSpill{e->keyLen + valLen > kInlineCap};

    // Reuse the spill buffer when the value doesn't grow; otherwise the key moves.
    char* old{wasSpilled ? e->bufPtr : nullptr};
    if (willSpill && (!wasSpilled || valLen > e->valLen)) {
      char* buf{new char[e->keyLen + valLen]};
      std::memcpy(buf, e->key().data(), e->keyLen);
      e->bufPtr = buf;  // overwrites inlineBuf, whose key was just copied out
    } else if (!willSpill && wasSpilled) {
      std::memcpy(e->inlineBuf.data(), old, e->keyLen);  // overwrites bufPtr; `old` kept it
    } else {
      old = nullptr;  // buffer unchanged, key already in place
    }

    char* dst{willSpill ? e->bufPtr : e->inlineBuf.data()};
    std::memcpy(dst + e->keyLen, value.data(), valLen);
    e->valLen = valLen;
    delete[] old;
  }

  void Table::remove(Entry* e) noexcept {
    // Points at whichever link holds e: the bucket slot or the previous entry's next.
    Entry** link{&buckets_[bucketOf(e->hash)]};
    while (*link != e) {
      assert(*link != nullptr && "Table::remove - remove of an entry not in the table");
      link = &(*link)->next;
    }
    *link = e->next;

    if (e->spilled()) {
      delete[] e->bufPtr;
    }
    pool_.deallocate(e);
    --liveCount_;
  }

  void Table::clear() noexcept {
    for (Entry*& head : buckets_) {
      for (Entry* e{head}; e != nullptr;) {
        Entry* next{e->next};
        if (e->spilled()) {
          delete[] e->bufPtr;
        }
        pool_.deallocate(e);
        e = next;
      }
      head = nullptr;
    }
    liveCount_ = 0;
  }

  std::size_t Table::sampleSome(std::span<Entry*> out, std::uint64_t& rngState) noexcept {
    if (size() == 0 || out.size() == 0) {
      return 0;
    }

    const std::size_t mask{bucketCount() - 1};
    // Capped at bucketCount so the walk never wraps onto a bucket it already took,
    // which keeps the output distinct.
    const std::size_t budget{std::min(out.size() * 10, bucketCount())};

    std::size_t written{0};
    auto idx{static_cast<std::size_t>(splitmix64(rngState)) & mask};
    for (std::size_t step{0}; step < budget && written < out.size(); ++step) {
      for (Entry* e{buckets_[idx]}; e != nullptr && written < out.size(); e = e->next) {
        out[written++] = e;
      }
      idx = (idx + 1) & mask;
    }
    return written;
  }

  std::size_t Table::size() const noexcept { return liveCount_; }

  std::size_t Table::bucketCount() const noexcept { return buckets_.size(); }

  std::size_t Table::used() const noexcept { return size(); }

  void Table::maybeGrow() {
    if (liveCount_ >= bucketCount()) {
      rehash(bucketCount() * 2);
    }
  }

  // Relinks nodes rather than copying them, so every Entry* stays valid across growth.
  void Table::rehash(std::size_t newBucketCount) {
    assert(std::has_single_bit(newBucketCount) && newBucketCount >= bucketCount());

    std::vector<Entry*> next(newBucketCount, nullptr);
    const std::size_t mask{newBucketCount - 1};

    for (Entry* head : buckets_) {
      for (Entry* e{head}; e != nullptr;) {
        Entry* after{e->next};
        Entry*& slot{next[e->hash & mask]};
        e->next = slot;
        slot = e;
        e = after;
      }
    }

    buckets_.swap(next);
  }

  std::size_t Table::bucketOf(std::uint32_t h) const noexcept { return h & (bucketCount() - 1); }

}  // namespace tinyredis
