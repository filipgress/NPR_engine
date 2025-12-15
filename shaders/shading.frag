#version 450

const float EPSILON = 0.001;

const uint SHADING_GOOCH = 2u;
const uint SHADING_TOON = 3u;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D light_map;
layout(set = 0, binding = 1) uniform sampler2D albedo_metallic;

layout(push_constant) uniform ShadingPC {
  vec4 gooch_warm; // rgb = warm color, a = unused
  vec4 gooch_cool; // rgb = cool color, a = unused

  uint shading_mode; // 2 = gooch, 3 = toon
  float gooch_alpha; // blend factor for warm
  float gooch_beta; // blend factor for cool

  uint toon_steps;
  float toon_min_brightness;
  float toon_threshold;
};

vec3 apply_gooch_shading(float intensity, vec3 albedo) {
  vec3 cool = gooch_cool.rgb + albedo * gooch_beta;
  vec3 warm = gooch_warm.rgb + albedo * gooch_alpha;

  // vec3 cool = gooch_cool.rgb * albedo * gooch_beta;
  // vec3 warm = gooch_warm.rgb * albedo * gooch_alpha;

  return mix(cool, warm, intensity);
}

vec3 apply_toon_shading(float intensity, vec3 albedo) {
  float steps = float(toon_steps);
  float quantized;

  if (toon_steps == 1)
    quantized = step(toon_threshold, intensity);
  else
    quantized = floor(intensity * steps) / steps;

  quantized = mix(toon_min_brightness, 1.0, quantized);
  return albedo * quantized;
}

void main() {
  vec3 albedo = texture(albedo_metallic, frag_uv).rgb;
  float total_int = texture(light_map, frag_uv).r;
  total_int = clamp(total_int, 0.0, 1.0);

  // out_color = vec4(total_int, total_int, total_int, 1.0); // debug
  // return;

  vec3 final_color;
  if (shading_mode == SHADING_GOOCH)
    final_color = apply_gooch_shading(total_int, albedo);
  else // SHADING_TOON
    final_color = apply_toon_shading(total_int, albedo);

  out_color = vec4(final_color, 1.0);
}
