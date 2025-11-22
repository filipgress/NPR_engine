#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec4 in_tangent;

layout(location = 4) in mat4 model;
layout(location = 8) in mat3 normal;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_pos;
layout(location = 2) out mat3 TBN;

layout(set = 0, binding = 0) uniform CameraUnif {
  mat4 view;
  mat4 proj;
  mat4 proj_view;
};

void main() {
  vec4 view_pos = view * model * vec4(in_pos, 1.0);

  frag_pos = view_pos.rgb;
  frag_uv = in_uv;

  mat3 normal_mat = mat3(view) * normal;

  vec3 N = normalize(normal_mat * in_normal);
  vec3 T = normalize(normal_mat * in_tangent.xyz);
  vec3 B = cross(N, T) * in_tangent.w;
  TBN = mat3(T, B, N);

  gl_Position = proj * view_pos;
}
