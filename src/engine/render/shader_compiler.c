#include "shader_compiler.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <errno.h>
#endif

#define SHADER_SOURCE_DIR "shaders"
#define SHADER_CACHE_DIR  "shader_cache"
#define SHADER_PATH_MAX   512

static void shader_error(const char *path, const char *message) {
  printf("\033[1;31m[Shader Error]\033[0m %s: %s\n", path, message);
}

/* Returns the modification time of a file, or -1 if it doesn't exist. */
static i64 shader_file_mtime(const char *path) {
#ifdef _WIN32
  struct _stat64 st;

  if (_stat64(path, &st) != 0)
    return -1;
#else
  struct stat st;

  if (stat(path, &st) != 0)
    return -1;
#endif

  return (i64)st.st_mtime;
}

static void shader_make_cache_dir(void) {
#ifdef _WIN32
  _mkdir(SHADER_CACHE_DIR);
#else
  mkdir(SHADER_CACHE_DIR, 0755);
#endif
}

static u8 *shader_read_file(const char *path, u32 *size_out) {
  FILE *f = fopen(path, "rb");

  if (f == NULL)
    return NULL;

  fseek(f, 0, SEEK_END);
  const long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size < 0) {
    fclose(f);
    return NULL;
  }

  u8 *data = (u8 *)malloc((size_t)size + 1);

  if (fread(data, 1, (size_t)size, f) != (size_t)size) {
    fclose(f);
    free(data);
    return NULL;
  }

  fclose(f);

  data[size] = 0; /* So sources can double as C strings */
  *size_out  = (u32)size;

  return data;
}

static u8 shader_write_file(const char *path, const u8 *data, const u32 size) {
  FILE *f = fopen(path, "wb");

  if (f == NULL)
    return 0;

  const u8 ok = fwrite(data, 1, size, f) == size;

  fclose(f);

  return ok;
}

static u8 *shader_compile(const char *src_path, const char *source, const ShaderStage stage, u32 *size_out) {
  static u8 glslang_initialized = 0;

  if (!glslang_initialized) {
    glslang_initialize_process();
    glslang_initialized = 1;
  }

  const glslang_stage_t glslang_stage = stage == SHADER_STAGE_VERTEX ? GLSLANG_STAGE_VERTEX : GLSLANG_STAGE_FRAGMENT;

  const glslang_input_t input = {
    .language                          = GLSLANG_SOURCE_GLSL,
    .stage                             = glslang_stage,
    .client                            = GLSLANG_CLIENT_VULKAN,
    .client_version                    = GLSLANG_TARGET_VULKAN_1_0,
    .target_language                   = GLSLANG_TARGET_SPV,
    .target_language_version           = GLSLANG_TARGET_SPV_1_0,
    .code                              = source,
    .default_version                   = 100,
    .default_profile                   = GLSLANG_NO_PROFILE,
    .force_default_version_and_profile = 0,
    .forward_compatible                = 0,
    .messages = GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT,
    .resource = glslang_default_resource(),
  };

  glslang_shader_t *shader = glslang_shader_create(&input);

  if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input)) {
    shader_error(src_path, glslang_shader_get_info_log(shader));
    glslang_shader_delete(shader);

    return NULL;
  }

  glslang_program_t *program = glslang_program_create();

  glslang_program_add_shader(program, shader);

  if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    shader_error(src_path, glslang_program_get_info_log(program));
    glslang_program_delete(program);
    glslang_shader_delete(shader);

    return NULL;
  }

  glslang_program_SPIRV_generate(program, glslang_stage);

  const char *spirv_messages = glslang_program_SPIRV_get_messages(program);

  if (spirv_messages != NULL && spirv_messages[0] != 0)
    printf("\033[1;33m[Shader]\033[0m %s: %s\n", src_path, spirv_messages);

  const u32 size = (u32)(glslang_program_SPIRV_get_size(program) * sizeof(u32));
  u8 *data       = (u8 *)malloc(size);

  glslang_program_SPIRV_get(program, (unsigned int *)data);

  glslang_program_delete(program);
  glslang_shader_delete(shader);

  *size_out = size;

  return data;
}

u8 *shader_load(const char *name, const ShaderStage stage, u32 *size_out) {
  char src_path[SHADER_PATH_MAX], cache_path[SHADER_PATH_MAX];

  snprintf(src_path, sizeof(src_path), "%s/%s", SHADER_SOURCE_DIR, name);
  snprintf(cache_path, sizeof(cache_path), "%s/%s.spv", SHADER_CACHE_DIR, name);

  const i64 src_mtime   = shader_file_mtime(src_path);
  const i64 cache_mtime = shader_file_mtime(cache_path);

  /* Cache hit: the cached SPIR-V exists and is not older than the source
   * (or there is no source at all, e.g. in a stripped-down install). */
  if (cache_mtime != -1 && (src_mtime == -1 || cache_mtime >= src_mtime)) {
    u8 *data = shader_read_file(cache_path, size_out);

    if (data != NULL)
      return data;
  }

  if (src_mtime == -1) {
    shader_error(src_path, "shader source not found (and no usable cached SPIR-V)");
    return NULL;
  }

  u32 source_size;
  u8 *source = shader_read_file(src_path, &source_size);

  if (source == NULL) {
    shader_error(src_path, "failed to read shader source");
    return NULL;
  }

  printf("[Shader] Compiling %s\n", src_path);

  u8 *data = shader_compile(src_path, (const char *)source, stage, size_out);

  free(source);

  if (data == NULL)
    return NULL;

  shader_make_cache_dir();

  if (!shader_write_file(cache_path, data, *size_out))
    printf("\033[1;33m[Shader]\033[0m %s: failed to write cache, will recompile next launch\n", cache_path);

  return data;
}

void shader_free(u8 *data) { free(data); }
