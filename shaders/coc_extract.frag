#version 450

layout(constant_id = 0) const int SAMPLES = 4;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out float out_coc;

layout(set = 0, binding = 0) uniform sampler2D depth_tex;

layout(push_constant) uniform CocPC {
  float focus_dist;
  float focus_range;

  float near_int;
  float far_int;

  float near_falloff;
  float far_falloff;

  float near_plane;
  float far_plane;

  uint is_persp; // 1 = presp, 0 = ortho
};

// linearize depth value from [0, 1] to view space depth
float linearize_depth(float depth) {
  if (is_persp == 1) {
    float z_ndc = depth * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) /
      (far_plane + near_plane - z_ndc * (far_plane - near_plane));
  } else {
    return mix(near_plane, far_plane, depth);
  }
}

void main() {
  float depth = texture(depth_tex, frag_uv).r;
  float linear_depth = linearize_depth(depth);

  float focus_near = focus_dist - focus_range * 0.5;
  float focus_far = focus_dist + focus_range * 0.5;

  float coc = 0.0;

  if (linear_depth < focus_near) // foreground
    coc = clamp((linear_depth - focus_near) / near_falloff, -1.0, 0.0) * near_int;
  else if (linear_depth > focus_far) // background
    coc = clamp((linear_depth - focus_far) / far_falloff, 0.0, 1.0) * far_int;

  out_coc = coc;
}
