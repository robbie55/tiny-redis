#include "core/table.h"

#include "core/hash.h"
#include "core/todo.h"

namespace tinyredis {

  Table::Table(std::size_t /*initialBuckets*/, std::uint64_t /*hashSeed*/) {
    unimplemented("Table::Table");
  }

  Table::~Table() { unimplemented("Table::~Table"); }

  Entry* Table::find(std::string_view /*key*/) noexcept { unimplemented("Table::find"); }

  Entry* Table::insert(std::string_view /*key*/, std::string_view /*value*/) {
    unimplemented("Table::insert");
  }

  void Table::assign(Entry* /*e*/, std::string_view /*value*/) { unimplemented("Table::assign"); }

  void Table::remove(Entry* /*e*/) noexcept { unimplemented("Table::remove"); }

  void Table::clear() noexcept { unimplemented("Table::clear"); }

  std::size_t Table::sampleSome(std::span<Entry*> /*out*/, std::uint64_t& /*rngState*/) noexcept {
    unimplemented("Table::sampleSome");
  }

  std::size_t Table::size() const noexcept { unimplemented("Table::size"); }

  std::size_t Table::bucketCount() const noexcept { unimplemented("Table::bucketCount"); }

  std::size_t Table::used() const noexcept { unimplemented("Table::used"); }

  void Table::maybeGrow() { unimplemented("Table::maybeGrow"); }

  void Table::rehash(std::size_t /*newBucketCount*/) { unimplemented("Table::rehash"); }

  std::size_t Table::bucketOf(std::uint64_t /*h*/) const noexcept {
    unimplemented("Table::bucketOf");
  }

}  // namespace tinyredis
