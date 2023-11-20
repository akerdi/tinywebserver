cmake_minimum_required(VERSION 3.6)

project(test)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  test_executable

  ../src/log/log.cpp
  ../src/CGImysql/sql_connection_pool.cpp

  lock/mutex.cpp
  lock/semaphore.cpp
  lock/cond.cpp
  log/block_queue.cpp
  log/log.cpp
  CGImysql/sql_connection_pool.cpp
)

target_link_libraries(
  test_executable
  gtest_main
  -lpthread
  -lmysqlclient
)

include(GoogleTest)
gtest_discover_tests(test_executable)