#pragma once

#include <cstddef>
#include <string>

namespace tinyredis {

  inline std::string key(std::size_t i) { return "key:" + std::to_string(i); }
  inline std::string val(std::size_t i) { return "val:" + std::to_string(i); }

}  // namespace tinyredis
