#version 460

layout (location = 0) in vec2 uv;

layout (location = 0) out vec2 v_uv;
layout (location = 1) out vec4 v_color;

layout (push_constant, std430) uniform constants {
  mat4 camera_matrix;
  vec4 quat;
  vec4 pos;
  vec4 color;
  vec2 scale;
  vec2 uvoffset;
} PushConstants;

const vec4 vertices[] = {
  vec4(-0.05, -0.05, 0, 1),
  vec4(-0.05, 0.05, 0, 1),
  vec4(0.05, -0.05, 0, 1),
  vec4(0.05, -0.05, 0, 1),
  vec4(-0.05, 0.05, 0, 1),
  vec4(0.05, 0.05, 0, 1)
};

vec4 quaternion_rotation(vec4 q) {
  return vec4(q.xyz * sin(q.w * 0.5), cos(q.w * 0.5));
}

mat4 quaternion_rotation_matrix(vec4 q) {
  return mat4(
    /* x */
    1.0 - 2.0 * q.y * q.y - 2.0 * q.z * q.z,
    2.0 * q.x * q.y + 2.0 * q.w * q.z,
    2.0 * q.x * q.z - 2.0 * q.w * q.y,
    0,
    /* y */
    2.0 * q.x * q.y -2.0 * q.w * q.z,
    1.0 - 2.0 * q.x * q.x - 2.0 * q.z * q.z,
    2.0 * q.y * q.z -2.0 * q.w * q.x,
    0,
    /* z */
    2.0 * q.x * q.z + 2.0 * q.w * q.y,
    2.0 * q.y * q.z + 2.0 * q.w * q.x,
    1.0 - 2.0 * q.x * q.x - 2.0 * q.y * q.y,
    0,
    /* w */
    0,
    0,
    0,
    1
  );
}

void main() {
  mat4 trans_mat = mat4(
    PushConstants.scale.x, 0, 0, 0,
    0, PushConstants.scale.y, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
  );

  trans_mat = trans_mat * quaternion_rotation_matrix(quaternion_rotation(PushConstants.quat));
  trans_mat[3] = PushConstants.pos;

  gl_Position = PushConstants.camera_matrix * trans_mat * vertices[gl_VertexIndex];
  v_uv = uv + PushConstants.uvoffset;
  v_color = PushConstants.color;
}
