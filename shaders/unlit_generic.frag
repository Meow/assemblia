#version 460

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 v_color;

layout (location = 0) out vec4 f_color;

layout (binding = 0) uniform sampler2D color_texture;

void main() {
  if (v_color.x != 1 || v_color.y != 1 || v_color.z != 1) {
    f_color = mix(texture(color_texture, uv), v_color, 0.5);
  } else {
    f_color = texture(color_texture, uv);
  }
}
