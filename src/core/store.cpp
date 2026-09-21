#include "core/store.h"

#include "core/todo.h"

namespace tinyredis {

  Store::Store() : Store(Config{}) {}

  Store::Store(const Config& /*cfg*/) { unimplemented("Store::Store"); }

  Store::~Store() = default;

  Entry* Store::find(std::string_view /*key*/, std::int64_t /*now*/) {
    unimplemented("Store::find");
  }

  bool Store::set(std::string_view /*key*/, std::string_view /*value*/, std::int64_t /*expireAtMs*/,
                  std::int64_t /*now*/) {
    unimplemented("Store::set");
  }

  bool Store::erase(std::string_view /*key*/, std::int64_t /*now*/) {
    unimplemented("Store::erase");
  }

  bool Store::setExpireAt(std::string_view /*key*/, std::int64_t /*expireAtMs*/,
                          std::int64_t /*now*/) {
    unimplemented("Store::setExpireAt");
  }

  std::int64_t Store::ttlMs(std::string_view /*key*/, std::int64_t /*now*/) {
    unimplemented("Store::ttlMs");
  }

  ExpireCycleResult Store::activeExpireCycle(std::int64_t /*now*/, std::size_t /*sampleLimit*/) {
    unimplemented("Store::activeExpireCycle");
  }

  void Store::clear() { unimplemented("Store::clear"); }

  std::size_t Store::size() const noexcept { unimplemented("Store::size"); }

  std::size_t Store::bucketCount() const noexcept { unimplemented("Store::bucketCount"); }

  std::size_t Store::maxKeys() const noexcept { unimplemented("Store::maxKeys"); }

}  // namespace tinyredis
