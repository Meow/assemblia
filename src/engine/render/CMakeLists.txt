set(engine_render_source_files
  render_state.c
  geyser.c
  memory.c
  renderable.c
  glyph.c
  render.c
  shader_compiler.c
)

set(engine_render_header_files
  render_state.h
  geyser.h
  memory.h
  renderable.h
  glyph.h
  render.h
  shader_compiler.h
)

set(GLAD_DIR ../../../vendor/glad)
set(GLAD_VK_FILES ${GLAD_DIR}/src/vulkan.c ${GLAD_DIR}/include/glad/vulkan.h)

include_directories("${GLAD_DIR}/include")

# glslang compiles GLSL to SPIR-V at runtime (see shader_compiler.c). Provided
# by the Vulkan SDK on Windows, the glslang Homebrew formula on macOS, and
# glslang/shaderc packages on Linux.
find_package(glslang CONFIG REQUIRED HINTS
  $ENV{VULKAN_SDK}
  /opt/homebrew/opt/glslang
  /usr/local/opt/glslang)

add_library(render ${engine_render_source_files} ${engine_render_header_files} ${GLAD_VK_FILES})
target_link_libraries(render input state glfw
  glslang::glslang
  glslang::SPIRV
  glslang::glslang-default-resource-limits)
