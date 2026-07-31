#include "renderable.h"

#include "../crc64.h"
#include "../input/asset.h"
#include "../util.h"

#include <math.h>
#include <string.h>

static const Vector2 null_vec2    = { 0.0f, 0.0f };
static const Vector2 one_vec2     = { 1.0f, 1.0f };
static const Vector3 null_vec3    = { 0.0f, 0.0f, 0.0f };
static const Vector4 null_vec4    = { 0.0f, 0.0f, 0.0f, 1.0f };
static const Quaternion null_quat = { 0.0f, 0.0f, 0.0f, 1.0f };

struct RenderableTexture {
  GeyserTexture *tex;
  u64 crc;
};

static struct RenderableTexture *RENDERABLE_TEXTURES = NULL;

// clang-format off

static const Matrix4 null_mat4 = {
  { 1.0f, 0.0f, 0.0f, 0.0f },
  { 0.0f, 1.0f, 0.0f, 0.0f },
  { 0.0f, 0.0f, 1.0f, 0.0f },
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

// clang-format on

void renderable_make_default(Renderable *r) {
  r->uv               = NULL;
  r->pool             = NULL;
  r->texture          = NULL;
  r->vertices_count   = 0U;
  r->position         = null_vec4;
  r->last_position    = null_vec4;
  r->rotation         = null_quat;
  r->transform_matrix = null_mat4;
  r->active           = GS_FALSE;
  r->should_zsort     = GS_FALSE;
  r->assigned_to      = -1;
  r->velocity         = null_vec3;
  r->scale            = one_vec2;
  r->uv_offset        = null_vec2;
  r->color            = (Vector4) { 1.0f, 1.0f, 1.0f, 1.0f };
  r->updated_at       = 0.0;
}

Renderable renderable_default() {
  Renderable r;

  renderable_make_default(&r);

  return r;
}

void renderable_send_memory(RenderState *state, Renderable *r) {
  const u64 uv_size = sizeof(Vector2) * r->vertices_count;
  void *data;

  VkBufferCopy copy_info = { .srcOffset = 0, .dstOffset = r->offset, .size = uv_size };

  geyser_success_or_message(
    vkMapMemory(state->device, state->memory, 0, uv_size, 0, &data), "Failed to map vertex memory!"
  );
  memcpy(data, r->uv, uv_size);
  vkUnmapMemory(state->device, state->memory);

  vkCmdCopyBuffer(state->command_buffer, state->buffer, r->pool->buffer, 1, &copy_info);

  geyser_cmd_submit_staging(state);
}

void renderable_make_rect(const RenderState *state, Renderable *r) {
  // clang-format off
  Vector2 uvmap[6] = {
    {0.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f}
  };
  // clang-format on

  r->uv = (Vector2 *)malloc(sizeof(Vector2) * 6);

  memcpy(r->uv, uvmap, sizeof(Vector2) * 6);

  r->vertices_count = 6U;
}

void renderable_make_rect_ex(const RenderState *state, Renderable *r, const f32 uv_width, const f32 uv_height) {
  // clang-format off
  Vector2 uvmap[6] = {
    {0.0f,     0.0f},
    {0.0f,     uv_height},
    {uv_width, 0.0f},
    {uv_width, 0.0f},
    {0.0f,     uv_height},
    {uv_width, uv_height}
  };
  // clang-format on

  r->uv = (Vector2 *)malloc(sizeof(Vector2) * 6);

  memcpy(r->uv, uvmap, sizeof(Vector2) * 6);

  r->vertices_count = 6U;
}

void renderable_free(RenderState *state, Renderable *r) {
  /* Renderables that never got memory assigned have no pool;
   * memory_free_block ignores them. */
  memory_free_block(r->pool, r->offset, renderable_get_size(r));
  /* TODO: fix memory freeing */
  // memory_free_image_block(r->texture.base.base.pool, r->texture.base.base.offset, r->texture.base.base.size);

  // if (r->texture.copy == 0)
  //   geyser_free_texture_descriptor_set(state, &r->texture);

  free(r->uv);
}

void renderable_calc_matrix(Renderable *r) {
  r->transform_matrix = null_mat4;

  /* Scale */
  r->transform_matrix.x[0] = r->scale.x;
  r->transform_matrix.y[1] = r->scale.y;

  /* Rotation */
  r->transform_matrix = matrix_mul(r->transform_matrix, quaternion_rotation_matrix(r->rotation));

  /* Translation */
  r->transform_matrix.w[0] = r->position.x;
  r->transform_matrix.w[1] = r->position.y;
  r->transform_matrix.w[2] = r->position.z;
}

void renderable_interpolate(Renderable *r) {
  const f32 delta = (f32)(platform_time_f64() - r->updated_at);

  r->position.x = util_lerp_f32(delta, r->last_position.x, r->last_position.x + r->velocity.x);
  r->position.y = util_lerp_f32(delta, r->last_position.y, r->last_position.y + r->velocity.y);
  r->position.z = util_lerp_f32(delta, r->last_position.z, r->last_position.z + r->velocity.z);
}

void renderable_set_pos(Renderable *r, const Vector4 pos) {
  r->position      = pos;
  r->last_position = pos;
}

void renderable_set_color(Renderable *r, const Vector4 col) { r->color = col; }

void renderable_set_rotation(Renderable *r, const Vector3 axis, const f32 rotation) {
  if (!vector_is_normal3(&axis))
    return;

  r->rotation = (Quaternion) { axis.x, axis.y, axis.z, rotation };
}

void renderable_set_velocity(Renderable *r, const Vector3 vel) { r->velocity = vel; }

GeyserTexture *_get_geyser_texture(const u64 crc) {
  if (crc == 0)
    return NULL;

  if (RENDERABLE_TEXTURES == NULL)
    RENDERABLE_TEXTURES = (struct RenderableTexture *)calloc(1024, sizeof(struct RenderableTexture));

  for (u16 i = 0; i < 1024; i++)
    if (RENDERABLE_TEXTURES[i].crc != 0 && RENDERABLE_TEXTURES[i].crc == crc)
      return RENDERABLE_TEXTURES[i].tex;

  return NULL;
}

void _add_geyser_texture(const u64 crc, GeyserTexture *tex) {
  if (crc == 0)
    return;

  if (RENDERABLE_TEXTURES == NULL)
    RENDERABLE_TEXTURES = (struct RenderableTexture *)calloc(1024, sizeof(struct RenderableTexture));

  for (u16 i = 0; i < 1024; i++) {
    if (RENDERABLE_TEXTURES[i].crc == 0) {
      RENDERABLE_TEXTURES[i].crc = crc;
      RENDERABLE_TEXTURES[i].tex = tex;

      return;
    }
  }
}

void renderable_load_texture(RenderState *state, Renderable *r, const char *image_path) {
  Image tex_img;
  asset_load_image(&tex_img, image_path);

  strcpy(r->texture_path, image_path);

  const u64 crc = crc64(tex_img.data, tex_img.width * tex_img.height * 4);

  GeyserTexture *tex = _get_geyser_texture(crc);

  if (tex == NULL) {
    r->texture = (GeyserTexture *)calloc(1, sizeof(GeyserTexture));

    geyser_create_texture(state, crc, vector_make2((f32)tex_img.width, (f32)tex_img.height), r->texture);

    if (r->texture->base.base.newblock == 1)
      geyser_set_image_memory(state, &r->texture->base.base, &tex_img);
    else
      geyser_set_image_memory_barrier(state, &r->texture->base.base);

    _add_geyser_texture(crc, r->texture);
  } else {
    r->texture = tex;
  }

  asset_unload_image(&tex_img);
}

void renderable_set_texture(RenderState *state, Renderable *r, const Image tex_img) {
  const u64 crc = crc64(tex_img.data, tex_img.width * tex_img.height * 4);

  GeyserTexture *tex = _get_geyser_texture(crc);

  if (tex == NULL) {
    r->texture = (GeyserTexture *)calloc(1, sizeof(GeyserTexture));

    geyser_create_texture(
      state,
      crc64(tex_img.data, tex_img.width * tex_img.height * 4),
      vector_make2((f32)tex_img.width, (f32)tex_img.height),
      r->texture
    );

    if (r->texture->base.base.newblock == 1)
      geyser_set_image_memory(state, &r->texture->base.base, &tex_img);
    else
      geyser_set_image_memory_barrier(state, &r->texture->base.base);

    _add_geyser_texture(crc, r->texture);
  } else {
    r->texture = tex;
  }
}

void renderable_init_rect(RenderState *state, Renderable *r) {
  renderable_make_rect(state, r);
  renderable_assign_memory(state, (MemoryManager *)state->memory_manager, r);
  renderable_send_memory(state, r);
}

void renderable_init_rect_ex(RenderState *state, Renderable *r, const f32 uv_width, const f32 uv_height) {
  renderable_make_rect_ex(state, r, uv_width, uv_height);
  renderable_assign_memory(state, (MemoryManager *)state->memory_manager, r);
  renderable_send_memory(state, r);
}

void renderable_set_scale(Renderable *r, const Vector2 scale) { r->scale = scale; }

void renderable_set_active(Renderable *r, GeyserBool active) { r->active = active; }

void renderable_set_updated(Renderable *r, const f64 updated_at) { r->updated_at = updated_at; }

void renderable_set_assigned(Renderable *r, i64 ent_id) { r->assigned_to = ent_id; }

void renderable_set_should_zsort(Renderable *r, const GeyserBool should_zsort) { r->should_zsort = should_zsort; }

void renderable_set_uv_offset(Renderable *r, const Vector2 offset) { r->uv_offset = offset; }

u64 renderable_get_size(const Renderable *r) { return sizeof(Vector2) * r->vertices_count; }

void renderable_assign_memory(RenderState *state, MemoryManager *m, Renderable *r) {
  const u64 size = renderable_get_size(r);
  FreeMemoryBlock mblock;

  memory_find_free_block(state, m, 0, size, &mblock);

  r->offset = mblock.offset;
  r->pool   = mblock.pool;
}
