#version 450

layout(constant_id = 0) const int SAMPLES = 4;

const float EPSILON = 0.001;

const uint SHADING_GOOCH = 2u;
const uint SHADING_TOON = 3u;

layout(location = 0) out float out_int;

layout(set = 1, binding = 0) uniform sampler2DMS g_albedo_metallic;
layout(set = 1, binding = 1) uniform sampler2DMS g_emissive_roughness;
layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;
layout(set = 1, binding = 4) uniform sampler2D g_coverage;

layout(set = 2, binding = 0) uniform SpotLightUnif {
  vec4 pos; // xyz = view-space position, w = range
  vec4 dir; // xyz = normalized view-space direction to light, w = unused
  vec4 color; // rgb = color * intensity, a = intensity
  vec4 params; // x = angle_scale, y = angle_offset, zw = unused
} light;

layout(push_constant) uniform LightPC {
  mat4 model; // unused

  uint shading_mode; // 2 = gooch, 3 = toon
  float diff_int;
  float spec_int;
};

float calc_light_intensity(int idx, ivec2 px) {
  vec3 normal = normalize(texelFetch(g_normal, px, idx).xyz);

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return 0.0;

  vec4 er = texelFetch(g_emissive_roughness, px, idx);
  float roughness = clamp(er.a, 0.05, 1.0);
  vec3 position = texelFetch(g_position, px, idx).xyz;

  vec3 light_vec = light.pos.xyz - position;
  float dist = length(light_vec);

  vec3 light_dir = normalize(light_vec);
  vec3 view_dir = normalize(-position);

  // attenuation
  float theta = dot(light_dir, normalize(light.dir.xyz));

  float spot_att = clamp(theta * light.params.x + light.params.y, 0.0, 1.0);
  float dist_att = 1.0 - smoothstep(0.0, light.pos.w, dist);
  spot_att *= spot_att;

  float total_att = dist_att * spot_att;

  float light_intensity = light.color.a * total_att;
  float total_int = 0.0;

  if (diff_int > EPSILON) {
    float ndotl = dot(normal, light_dir);
    float t;

    if (shading_mode == SHADING_GOOCH)
      t = (ndotl + 1.0) * 0.5;
    else // SHADING_TOON
      t = max(ndotl, 0.0);

    total_int += t * light_intensity * diff_int;
  }

  if (spec_int > EPSILON) {
    vec3 halfway = normalize(light_dir + view_dir);
    float ndoth = max(dot(normal, halfway), 0.0);
    float shininess = max((1.0 - roughness) * 256.0, 1.0);
    float spec = pow(ndoth, shininess);

    total_int += spec * light_intensity * spec_int;
  }

  return total_int;
}

void main() {
  vec2 coord = gl_FragCoord.xy;
  vec2 frag_uv = coord / textureSize(g_albedo_metallic);

  float coverage = texture(g_coverage, frag_uv).r;
  ivec2 px = ivec2(gl_FragCoord.xy);

  if (coverage == 1.0) { // simple pixel
    out_int = calc_light_intensity(0, px);
    return;
  }

  // complex pixel - average across samples
  float total_int = 0.0;
  for (int s = 0; s < SAMPLES; ++s)
    total_int += calc_light_intensity(s, px);

  out_int = total_int / float(SAMPLES);
}
