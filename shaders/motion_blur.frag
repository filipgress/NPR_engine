#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(set = 1, binding = 0) uniform sampler2D velocity_tex;
layout(set = 2, binding = 0) uniform sampler2D depth_tex;

layout(push_constant) uniform MotionBlurPC {
  float blur_strength;
  uint num_samples;
  float max_velocity;
  uint debug_mode;
  float depth_threshold;

  float near;
  float far;
  uint is_persp; // 1 = presp, 0 = ortho
};

// linearize depth value from [0, 1] to view space depth
float linearize_depth(float depth) {
  if (is_persp == 1)
    return near * far / (far - depth * (far - near));
  else
    return near + depth * (far - near);
}

void main() {
  vec2 velocity = texture(velocity_tex, frag_uv).rg * blur_strength;

  float vel_length = length(velocity);
  if (vel_length > max_velocity) {
    velocity = velocity / vel_length * max_velocity;
    vel_length = max_velocity;
  }

  if (debug_mode == 1u) {
    out_color = vec4(abs(velocity) * 10.0, 0.0, 1.0);
    return;
  }

  if (vel_length < 0.0001) {
    out_color = texture(color_tex, frag_uv);
    return;
  }

  float center_depth = texture(depth_tex, frag_uv).r;
  float center_linear = linearize_depth(center_depth);

  vec3 color = vec3(0.0);
  float total_weight = 0.0;

  for (uint i = 0u; i < num_samples; ++i) {
    float t = float(i) / float(num_samples - 1u) - 0.5;
    // float t = float(i) / float(num_samples - 1u);

    vec2 sample_uv = clamp(frag_uv + velocity * t, vec2(0.001), vec2(0.999));

    float sample_depth = texture(depth_tex, sample_uv).r;
    float sample_linear = linearize_depth(sample_depth);
    vec3 sample_color = texture(color_tex, sample_uv).rgb;

    float depth_diff = sample_linear - center_linear;
    float depth_weight = (depth_diff > 0.0)
      ? 1.0 - smoothstep(0.0, depth_threshold, depth_diff) : 1.0;
    // float depth_diff = abs(sample_linear - center_linear);
    // float depth_weight = 1.0 - smoothstep(0.0, depth_threshold, depth_diff);

    float dist_weight = 1.0 - abs(t * 2.0);
    // float dist_weight = 1.0 - t;

    float weight = dist_weight * depth_weight;

    color += sample_color * weight;
    total_weight += weight;
  }

  out_color = vec4(color / total_weight, 1.0);
}
