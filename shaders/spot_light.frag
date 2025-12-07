#version 450

layout(constant_id = 0) const int SAMPLES = 4;

const float EPSILON = 0.001;
const float PI = 3.14159265359;

const uint SHADING_BLINN_PHONG = 0u;
const uint SHADING_PBR = 1u;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2DMS g_albedo_metallic;
layout(set = 1, binding = 1) uniform sampler2DMS g_emissive_roughness;
layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;
layout(set = 1, binding = 4) uniform sampler2D g_coverage;

layout(set = 2, binding = 0) uniform SpotLightUnif {
  vec4 pos; // xyz = view-space position, w = range
  vec4 dir; // xyz = normalized view-space direction to light, w = unused
  vec4 color; // rgb = color, a = intensity
  vec4 params; // x = angle_scale, y = angle_offset, zw = unused
} light;

layout(push_constant) uniform LightPC {
  mat4 model; // unused

  uint shading_mode; // 0 = blinn-phong, 1 = pbr
  float diff_int;
  float spec_int;
};

float distribution_ggx(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float ndoth = max(dot(N, H), 0.0);
  float ndoth2 = ndoth * ndoth;

  float denom = (ndoth2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return a2 / denom;
}

float geometry_schlick_ggx(float ndotv, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float denom = ndotv * (1.0 - k) + k;

  return ndotv / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
  float ndotv = max(dot(N, V), 0.0);
  float ndotl = max(dot(N, L), 0.0);
  float ggx2 = geometry_schlick_ggx(ndotv, roughness);
  float ggx1 = geometry_schlick_ggx(ndotl, roughness);

  return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 calc_pbr(int idx, ivec2 px) {
  vec3 normal = normalize(texelFetch(g_normal, px, idx).xyz);

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return vec3(0.0);

  vec4 am = texelFetch(g_albedo_metallic, px, idx);
  vec3 albedo = am.rgb;
  float metallic = am.a;

  vec4 er = texelFetch(g_emissive_roughness, px, idx);
  float roughness = clamp(er.a, 0.05, 1.0);

  vec3 position = texelFetch(g_position, px, idx).xyz;

  vec3 light_vec = light.pos.xyz - position;
  float dist = length(light_vec);
  vec3 light_dir = normalize(light_vec);
  vec3 view_dir = normalize(-position);

  // attenuation
  float theta = dot(light_dir, normalize(light.dir.xyz));

  float dist_att = 1.0 - smoothstep(0.0, light.pos.w, dist);
  float spot_att = clamp(theta * light.params.x + light.params.y, 0.0, 1.0);
  spot_att *= spot_att;

  vec3 radiance = light.color.rgb * dist_att * spot_att;

  vec3 halfway = normalize(view_dir + light_dir);

  float ndotl = max(dot(normal, light_dir), 0.0);
  if (ndotl <= 0.0)
    return vec3(0.0);

  float ndotv = max(dot(normal, view_dir), 0.0);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  float NDF = distribution_ggx(normal, halfway, roughness);
  float G = geometry_smith(normal, view_dir, light_dir, roughness);
  vec3 F = fresnel_schlick(max(dot(halfway, view_dir), 0.0), F0);

  vec3 numerator = NDF * G * F;
  float denominator = 4.0 * ndotv * ndotl + EPSILON;
  vec3 specular = numerator / denominator;

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= 1.0 - metallic;

  return (kD * albedo / PI + specular) * radiance * ndotl;
}

vec3 calc_blinn_phong(int idx, ivec2 px) {
  vec3 normal = normalize(texelFetch(g_normal, px, idx).xyz);

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return vec3(0.0);

  vec3 albedo = texelFetch(g_albedo_metallic, px, idx).rgb;
  vec4 er = texelFetch(g_emissive_roughness, px, idx);
  float roughness = clamp(er.a, 0.05, 1.0);
  vec3 position = texelFetch(g_position, px, idx).xyz;

  vec3 light_vec = light.pos.xyz - position;
  float dist = length(light_vec);

  vec3 light_dir = normalize(light_vec);
  vec3 view_dir = normalize(-position);

  // attenuation
  float theta = dot(light_dir, normalize(light.dir.xyz));

  float dist_att = 1.0 - smoothstep(0.0, light.pos.w, dist);
  float spot_att = clamp(theta * light.params.x + light.params.y, 0.0, 1.0);
  spot_att *= spot_att;

  vec3 att_color = light.color.rgb * dist_att * spot_att;

  // diffuse
  float ndotl = max(dot(normal, light_dir), 0.0);
  vec3 diffuse = albedo * att_color * ndotl * diff_int;

  // specular
  vec3 specular = vec3(0.0);
  if (ndotl > 0.0 && spec_int > EPSILON) {
    vec3 halfway = normalize(light_dir + view_dir);
    float ndoth = max(dot(normal, halfway), 0.0);
    float shininess = max((1.0 - roughness) * 256.0, 1.0);
    float spec = pow(ndoth, shininess);
    specular = albedo * att_color * spec * spec_int;
  }

  return diffuse + specular;
}

void main() {
  vec2 coord = gl_FragCoord.xy;
  vec2 frag_uv = coord / textureSize(g_albedo_metallic);

  float coverage = texture(g_coverage, frag_uv).r;
  ivec2 px = ivec2(gl_FragCoord.xy);

  if (coverage == 1.0) { // simple pixel
    vec3 shaded_color;

    if (shading_mode == SHADING_PBR)
      shaded_color = calc_pbr(0, px);
    else // SHADING_BLINN_PHONG
      shaded_color = calc_blinn_phong(0, px);

    out_color = vec4(shaded_color, 1.0);
    return;
  }

  // complex pixel
  vec3 final_color = vec3(0.0);
  for (int s = 0; s < SAMPLES; ++s) {
    if (shading_mode == SHADING_PBR)
      final_color += calc_pbr(s, px);
    else // SHADING_BLINN_PHONG
      final_color += calc_blinn_phong(s, px);
  }

  out_color = vec4(final_color / float(SAMPLES), 1.0);
}
