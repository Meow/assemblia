#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 v_color;
layout (location = 2) flat in int v_tex;

layout (location = 0) out vec4 f_color;

/* Must stay in sync with GEYSER_RENDERABLE_TEXTURE_SLOTS in geyser.h */
layout (set = 0, binding = 1) uniform sampler2D color_textures[16];

void main() {
  vec4 tex_color = texture(color_textures[nonuniformEXT(v_tex)], uv);

  if (v_color.x != 1 || v_color.y != 1 || v_color.z != 1) {
    f_color = mix(tex_color, v_color, 0.5);
  } else {
    f_color = tex_color;
  }
}
