cmake_minimum_required(VERSION 3.13)
project(FileSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

include_directories(include)
add_library(filesystem
  src/directory.cpp
  src/disk.cpp
  src/file.cpp
  src/user.cpp

  # src/command/read.cpp
  # src/command/load.cpp
  )

link_libraries(filesystem)
add_executable(FileSystem main.cpp)
add_executable(test_maxlength test/test_maxlength.cpp)
add_executable(test_maxdisk test/test_maxdisk.cpp)
add_executable(test_alloc test/test_alloc.cpp)