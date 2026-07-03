cmake_minimum_required(VERSION 3.16)

project(miniflow_bin C CXX)

if (NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS"${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

if (WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
  set(CMAKE_CXX_FLAGS"${CMAKE_CXX_FLAGS} /W3")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /O2")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /O2")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED 11)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if (NOT WIN32 AND NOT APPLE)
  set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

if (APPLE)
  # GLFW's Cocoa backend is the only option here; Wayland/X11 must stay off.
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_COCOA ON CACHE BOOL "" FORCE)
endif()

find_package(Threads REQUIRED)

set(engine_source_files
  types/vector.c
  types/vector.h
  types/matrix.c
  types/matrix.h
  types/quaternion.c
  types/quaternion.h
  platform.c
  platform.h
  util.h
  limits.h
  crc64.c
  crc64.h
  main.c
)

set(GLFW_DIR ../../vendor/glfw)
set(GLAD_DIR ../../vendor/glad)
set(VULKAN_HEADERS_DIR ../../vendor/vulkan-headers)
set(STB_DIR ../../vendor/stb)
set(JSON_DIR ../../vendor/simdjson)
set(LUA_DIR ../../vendor/luajit/src)

include_directories("..")
include_directories("${GLAD_DIR}/include")
include_directories("${VULKAN_HEADERS_DIR}/include")
include_directories("${STB_DIR}/")

add_subdirectory(../../vendor/glfw ../../../build/glfw)

if (APPLE)
  # The renderer loads Vulkan entry points through GLFW, which dlopen()s the
  # Vulkan loader by bare name. dlopen() doesn't search Homebrew's prefix, so
  # hand GLFW an absolute path to the MoltenVK-backed loader instead. This
  # makes the binary run without VULKAN_SDK / DYLD_LIBRARY_PATH set.
  find_library(VULKAN_LOADER_LIB
    NAMES vulkan vulkan.1 libvulkan.1.dylib
    HINTS
      $ENV{VULKAN_SDK}/lib
      /opt/homebrew/lib
      /opt/homebrew/opt/vulkan-loader/lib
      /usr/local/lib
      /usr/local/opt/vulkan-loader/lib)

  if (VULKAN_LOADER_LIB)
    message(STATUS "Using Vulkan loader: ${VULKAN_LOADER_LIB}")
    target_compile_definitions(glfw PRIVATE _GLFW_VULKAN_LIBRARY="${VULKAN_LOADER_LIB}")
  else()
    message(WARNING "Vulkan loader (libvulkan) not found; install the vulkan-loader "
                    "and molten-vk Homebrew formulae, then reconfigure.")
  endif()
endif()

add_subdirectory(render ../../../build/render)
add_subdirectory(state ../../../build/state)
add_subdirectory(input ../../../build/input)
add_subdirectory(logic ../../../build/logic)
add_subdirectory(../game ../../../build/game)

set_target_properties(game PROPERTIES
  IMPORTED_LOCATION ../../../build/game/libgame.a
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
)

add_executable(miniflow ${engine_source_files})

target_link_libraries(miniflow PRIVATE render logic input state glfw Threads::Threads game)
