#version 450

layout(constant_id = 0) const int MAX_POISSON_SIZE = 64;

const float EPSILON = 0.001;
const float PI2 = 6.28318530718;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(set = 1, binding = 0) uniform sampler2D coc_map;
layout(set = 2, binding = 0) uniform sampler2D blue_noise_tex;
layout(set = 2, binding = 1) uniform PoissonDisk {
  uvec4 count; // x = count, yzw = unused
  vec4 samples[MAX_POISSON_SIZE]; // xy = offset, zw = unused
};

layout(push_constant) uniform DofPC {
  float blur_radius;
  float coc_threshold;
  float coc_falloff;
  uint debug_mode;
};

void main() {
  float center_coc = texture(coc_map, frag_uv).r;

  float radius = abs(center_coc) * blur_radius;
  if (radius < 0.3) { // sharp pixel
    out_color = texture(color_tex, frag_uv);
    return;
  }

  if (debug_mode == 1) {
    if (center_coc < -EPSILON)
      out_color = vec4(abs(center_coc), 0.0, 0.0, 1.0);
    else if (center_coc > EPSILON)
      out_color = vec4(0.0, 0.0, abs(center_coc), 1.0);
    return;
  }

  if (debug_mode == 2 && frag_uv.x < 0.5) {
    if (center_coc < -EPSILON)
      out_color = vec4(abs(center_coc), 0.0, 0.0, 1.0);
    else if (center_coc > EPSILON)
      out_color = vec4(0.0, 0.0, abs(center_coc), 1.0);
    return;
  }

  vec2 noise_scale = vec2(textureSize(coc_map, 0)) / vec2(textureSize(blue_noise_tex, 0));
  vec2 noise = texture(blue_noise_tex, frag_uv * noise_scale).rg;
  float angle = noise.r * PI2; // 0 to 2π

  float cos_angle = cos(angle);
  float sin_angle = sin(angle);
  mat2 rotation = mat2(cos_angle, -sin_angle, sin_angle, cos_angle);

  vec2 texel_size = 1.0 / vec2(textureSize(coc_map, 0));
  vec4 color = vec4(0.0);
  float total_weight = 0.0;

  for (uint i = 0; i < count.x; i++) {
    vec2 rotated = rotation * samples[i].xy;
    vec2 offset = rotated * radius * texel_size;

    vec2 sample_uv = frag_uv + offset;

    float sample_coc = texture(coc_map, sample_uv).r;
    vec4 sample_color = texture(color_tex, sample_uv);

    // dont sample background when in foreground
    if (center_coc < -EPSILON && sample_coc > EPSILON)
      continue;

    float coc_diff = abs(sample_coc - center_coc);
    float coc_weight = (coc_diff <= coc_threshold) ?
      1.0 : exp(-coc_diff * coc_falloff);

    float dist = length(rotated);
    float dist_weight = exp(-dist * dist * 2.0);

    float weight = coc_weight * dist_weight;

    color += sample_color * weight;
    total_weight += weight;
  }

  if (debug_mode == 3) {
    out_color = vec4(total_weight * 0.02);
    return;
  }

  out_color = color / max(total_weight, EPSILON);
}
