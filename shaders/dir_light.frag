#version 450

layout(constant_id = 0) const int SAMPLES = 4;
layout(constant_id = 1) const int MAX_DIR_LIGHTS = 2;

const float EPSILON = 0.001;
const float PI = 3.14159265359;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D ao_tex;

layout(set = 1, binding = 0) uniform sampler2DMS g_albedo_metallic;
layout(set = 1, binding = 1) uniform sampler2DMS g_emissive_roughness;
layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;
layout(set = 1, binding = 4) uniform sampler2D g_coverage;

struct DirLight {
  vec4 dir; // xyz = normalized view-space direction to light, w = unused
  vec4 color; // rgb = color * intensity, a = unused
};

layout(set = 2, binding = 0) uniform DirLightUnif {
  vec4 ambient; // rgb = color * intensity, a = intensity
  vec4 rim; // rgb = color * intensity, a = intensity

  float diff_int;
  float spec_int;
  float rim_power;
  uint inv_rim; // 0 = normal rim, 1 = inverse rim

  uvec4 count; // x = count, yzw = unused

  DirLight dir_lights[MAX_DIR_LIGHTS];
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

vec3 calc_pbr(int idx, ivec2 coord, vec3 ao) {
  vec3 normal = texelFetch(g_normal, coord, idx).xyz;

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return vec3(0.0);

  vec4 am = texelFetch(g_albedo_metallic, coord, idx);
  vec3 albedo = am.rgb;
  float metallic = am.a;

  vec4 er = texelFetch(g_emissive_roughness, coord, idx);
  vec3 emissive = er.rgb;
  float roughness = clamp(er.a, 0.05, 1.0);

  vec3 position = texelFetch(g_position, coord, idx).xyz;
  vec3 view_dir = normalize(-position);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  vec3 ambient_light = ao * albedo;
  vec3 Lo = vec3(0.0);

  for (int i = 0; i < count.x; ++i) {
    vec3 light_dir = dir_lights[i].dir.xyz;
    vec3 halfway = normalize(view_dir + light_dir);
    vec3 radiance = dir_lights[i].color.rgb;

    float ndotl = max(dot(normal, light_dir), 0.0);
    if (ndotl <= 0.0)
      continue;

    float NDF = distribution_ggx(normal, halfway, roughness);
    float G = geometry_smith(normal, view_dir, light_dir, roughness);
    vec3 F = fresnel_schlick(max(dot(halfway, view_dir), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, view_dir), 0.0) * max(dot(normal, light_dir), 0.0) + EPSILON;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    Lo += (kD * albedo / PI + specular) * radiance * ndotl;
  }

  return ambient_light + Lo + emissive;
}

vec3 calc_rim_light(vec3 view_dir, vec3 normal, vec3 albedo) {
  if (rim.a < EPSILON) return vec3(0.0);

  float ndotv = max(dot(view_dir, normal), 0.0);
  float rim_dot = inv_rim == 1u ? ndotv : (1.0 - ndotv);

  return albedo * rim.rgb * pow(rim_dot, rim_power);
}

vec3 calc_blinn_phong(int idx, ivec2 coord, vec3 ao) {
  vec3 normal = texelFetch(g_normal, coord, idx).xyz;

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return vec3(0.0);

  vec3 albedo = texelFetch(g_albedo_metallic, coord, idx).rgb;
  vec4 er = texelFetch(g_emissive_roughness, coord, idx);
  vec3 emissive = er.rgb;
  float roughness = clamp(er.a, 0.05, 1.0);
  vec3 position = texelFetch(g_position, coord, idx).xyz;

  vec3 view_dir = normalize(-position);
  vec3 rim_color = calc_rim_light(view_dir, normal, albedo);

  vec3 color = ao * albedo + rim_color + emissive; // ambient + rim + emissive
  float shininess = (1.0 - roughness) * 256.0;

  if (diff_int < EPSILON && spec_int < EPSILON)
    return color;

  for (int i = 0; i < count.x; ++i) {
    vec3 light_dir = dir_lights[i].dir.xyz;
    vec3 albedo_light = albedo * dir_lights[i].color.rgb;

    // diffuse
    vec3 diffuse = vec3(0.0);
    if (diff_int >= EPSILON) {
      float ndotl = max(dot(normal, light_dir), 0.0);
      diffuse = albedo_light * ndotl * diff_int;
    }

    // specular
    vec3 specular = vec3(0.0);
    if (spec_int >= EPSILON) {
      vec3 halfway = normalize(light_dir + view_dir);
      float ndoth = max(dot(normal, halfway), 0.0);
      float spec = pow(ndoth, shininess);
      specular = albedo_light * spec * spec_int;
    }

    color += diffuse + specular;
  }

  return color;
}

void main() {
  ivec2 coord = ivec2(gl_FragCoord.xy);

  vec3 ao = vec3(0.0);
  if (ambient.a >= EPSILON)
    ao = ambient.rgb * texture(ao_tex, frag_uv).r;

  float coverage = texture(g_coverage, frag_uv).r;
  if (coverage == 1.0) { // simple pixel
    out_color = vec4(calc_pbr(0, coord, ao), 1.0);
    // out_color = vec4(calc_blinn_phong(0, coord, ao), 1.0);
    return;
  }

  // complex pixel
  vec3 final_color = vec3(0.0);
  for (int s = 0; s < SAMPLES; ++s)
    final_color += calc_pbr(s, coord, ao);
  // final_color += calc_blinn_phong(s, coord, ao);

  out_color = vec4(final_color / float(SAMPLES), 1.0);
}
