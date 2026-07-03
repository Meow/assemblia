cmake_minimum_required(VERSION 3.16)

# Some vendored dependencies (e.g. luajit-cmake) still declare a
# cmake_minimum_required below 3.5, which CMake 4+ rejects outright. Allow them
# to configure by supplying a compatible policy floor.
if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(miniflow C)

set(LUAJIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit)

include(./vendor/luajit-cmake/LuaJIT.cmake)

add_subdirectory(./src/engine ../build/engine)
