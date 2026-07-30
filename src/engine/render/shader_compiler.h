#ifndef __ENGINE_RENDER_SHADER_COMPILER_H
#define __ENGINE_RENDER_SHADER_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../types/numeric.h"

typedef enum ShaderStage {
  SHADER_STAGE_VERTEX,
  SHADER_STAGE_FRAGMENT,
} ShaderStage;

/**
 * @brief Returns the SPIR-V code for a GLSL shader source, compiling it if needed.
 *
 * Looks for the source at `shaders/<name>` (relative to the working directory)
 * and a cached build at `shader_cache/<name>.spv`. If the cache is missing or
 * older than the source, the source is compiled with glslang and the cache is
 * rewritten; otherwise the cached SPIR-V is returned as-is. If the source is
 * absent but a cache exists (e.g. a stripped-down install), the cache is used.
 *
 * @param name Shader file name, e.g. "unlit_generic.vert".
 * @param stage Pipeline stage the shader belongs to.
 * @param size_out Receives the SPIR-V size in bytes.
 * @return u8* Malloc'd SPIR-V bytes (release with shader_free), or NULL on failure.
 */
u8 *shader_load(const char *name, const ShaderStage stage, u32 *size_out);

void shader_free(u8 *data);

#ifdef __cplusplus
}
#endif

#endif
