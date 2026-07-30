#version 460

struct GlyphData {
  vec4 pos;
  vec4 color;
  vec2 uvoffset;
  vec2 scale;
};

layout(std430, set = 0, binding = 0) readonly buffer GlyphBuffer {
  GlyphData data[];
} glyph_buffer;

layout (push_constant, std430) uniform constants {
  mat4 camera_matrix;
} push_constants;

layout (location = 0) out vec2 v_uv;
layout (location = 1) out vec4 v_color;

const vec4 vertices[] = {
  vec4(-0.05, -0.05, 0, 1),
  vec4(-0.05, 0.05, 0, 1),
  vec4(0.05, -0.05, 0, 1),
  vec4(0.05, -0.05, 0, 1),
  vec4(-0.05, 0.05, 0, 1),
  vec4(0.05, 0.05, 0, 1)
};

const vec2 uvs[] = {
  vec2(0, 0),
  vec2(0, 1.0 / 101.0),
  vec2(1, 0),
  vec2(1, 0),
  vec2(0, 1.0 / 101.0),
  vec2(1, 1.0 / 101.0)
};

void main() {
  mat4 trans_mat = mat4(
    glyph_buffer.data[gl_InstanceIndex].scale.x, 0, 0, 0,
    0, glyph_buffer.data[gl_InstanceIndex].scale.y, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
  );

  trans_mat[3] = glyph_buffer.data[gl_InstanceIndex].pos;

  gl_Position = push_constants.camera_matrix * trans_mat * vertices[gl_VertexIndex];
  v_uv = uvs[gl_VertexIndex] + glyph_buffer.data[gl_InstanceIndex].uvoffset;
  v_color = glyph_buffer.data[gl_InstanceIndex].color;
}
