cmake_minimum_required(VERSION 3.25)
project(tiny_redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(CTest)

option(TINYREDIS_BUILD_TESTS "Build the unit test suite" ON)
option(TINYREDIS_BUILD_BENCH "Build the pipelined benchmark client" ON)

# The warning set rides on an INTERFACE target rather than being set directory-wide, so
# -Werror applies to this project's translation units only. A directory-wide
# add_compile_options would also be inherited by everything FetchContent pulls in and let
# a dependency's headers fail our build.
add_library(tinyredis_warnings INTERFACE)
target_compile_options(tinyredis_warnings INTERFACE
  -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wfloat-conversion -pedantic)

include(FetchContent)

# robbie55/pool-allocator is header-only. SOURCE_SUBDIR deliberately names a directory
# that does not exist: FetchContent then populates the sources but skips
# add_subdirectory() on the upstream CMakeLists, which would otherwise build its demo and
# benchmark targets and drag doctest into our build graph.
FetchContent_Declare(pool_allocator
  GIT_REPOSITORY https://github.com/robbie55/pool-allocator.git
  GIT_TAG        36f63318d2dd2f2b0fab5a9d0ede7139ba3014d5
  SOURCE_SUBDIR  header-only-no-cmake
)
FetchContent_MakeAvailable(pool_allocator)

add_library(pool_allocator_headers INTERFACE)
target_include_directories(pool_allocator_headers SYSTEM INTERFACE ${pool_allocator_SOURCE_DIR})

# ---- portable core (builds and is unit-tested on Linux and macOS alike) --------------

add_library(tinyredis_core STATIC
  src/core/buffer.cpp
  src/core/entry.cpp
  src/core/hash.cpp
  src/core/resp.cpp
  src/core/store.cpp
  src/core/command.cpp
)
target_include_directories(tinyredis_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tinyredis_core
  PUBLIC  pool_allocator_headers
  PRIVATE tinyredis_warnings)

# ---- Linux-only networking layer ------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_library(tinyredis_net STATIC
    src/net/event_loop.cpp
    src/net/connection.cpp
    src/net/server.cpp
  )
  target_link_libraries(tinyredis_net
    PUBLIC  tinyredis_core
    PRIVATE tinyredis_warnings)

  add_executable(tiny-redis-server src/main.cpp)
  target_link_libraries(tiny-redis-server PRIVATE tinyredis_net tinyredis_warnings)
else()
  message(STATUS "tiny-redis: epoll is Linux-only; building the portable core and tests only")
endif()

# ---- tests ----------------------------------------------------------------------------

if(TINYREDIS_BUILD_TESTS AND BUILD_TESTING)
  # SYSTEM marks GoogleTest's include directory as a system path so our -Werror is not
  # tripped by warnings inside the framework's own headers.
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    SYSTEM
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(tinyredis_tests
    tests/test_buffer.cpp
    tests/test_resp.cpp
    tests/test_store.cpp
    tests/test_ttl.cpp
    tests/test_commands.cpp
  )
  target_link_libraries(tinyredis_tests
    PRIVATE tinyredis_core tinyredis_warnings GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(tinyredis_tests)
endif()

# ---- benchmark client (Day 3) -----------------------------------------------------------

if(TINYREDIS_BUILD_BENCH AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench/bench_client.cpp)
  find_package(Threads REQUIRED)
  add_executable(tinyredis_bench bench/bench_client.cpp)
  target_link_libraries(tinyredis_bench PRIVATE tinyredis_core tinyredis_warnings Threads::Threads)
endif()
