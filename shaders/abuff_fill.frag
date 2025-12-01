#version 450
layout(early_fragment_tests) in; // force early depth test

layout(constant_id = 0) const uint SAMPLES = 4;
layout(constant_id = 1) const uint MAX_TEXTURES = 128;
layout(constant_id = 2) const int MAX_DIR_LIGHTS = 2;

const float EPSILON = 0.001;
const float PI = 3.14159265359;
const uint MATERIAL_DOUBLE_SIDED = 1 << 0;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_pos; // view space position
layout(location = 2) in mat3 TBN;

struct ABuffNode {
  vec4 color;

  float depth;
  uint next;
  uint _padding[2];
};

layout(set = 1, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

layout(set = 2, binding = 0) buffer ABuffNodes {
  ABuffNode nodes[];
};
layout(set = 2, binding = 1) buffer HeadPointers {
  uint heads[];
};
layout(set = 2, binding = 2) buffer NodeCounter {
  uint counter;
};

struct DirLight {
  vec4 dir; // xyz = normalized view-space direction to light, w = unused
  vec4 color; // rgb = color * intensity, a = unused
};

layout(set = 3, binding = 0) uniform DirLightUnif {
  vec4 ambient; // rgb = color * intensity, a = intensity
  vec4 rim; // rgb = color * intensity, a = intensity

  float rim_power;
  uint inv_rim; // 0 = normal, 1 = inverse
  uint count;
  uint use_ssao;

  DirLight dir_lights[MAX_DIR_LIGHTS];
};

layout(set = 4, binding = 0) uniform MaterialUnif {
  ivec4 maps; // x=albedo, y=normal, z=metallic_roughness, w=emissive
  vec4 color_factor; // rgb = albedo, a = alpha
  vec4 emissive_factor; // rgb = emissive, a = unused

  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
} material;

layout(push_constant) uniform ABuffFillPC {
  uint width;
  uint max_nodes;
  float alpha_cutoff;

  float diff_int;
  float spec_int;
  uint is_pbr; // 0 = blinn-phong, 1 = pbr
};

// ============================================================================
// PBR Functions
// ============================================================================

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

// ============================================================================
// Lighting Calculations
// ============================================================================

vec3 calc_pbr(vec3 position, vec3 normal, vec3 albedo, float metallic,
  float roughness, vec3 emissive, vec3 ao) {
  vec3 view_dir = normalize(-position);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  vec3 ambient_light = ao * albedo;
  vec3 Lo = vec3(0.0);

  // Directional lights
  for (uint i = 0; i < count; ++i) {
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
  if (rim.a < EPSILON)
    return vec3(0.0);

  float ndotv = max(dot(view_dir, normal), 0.0);
  float rim_dot = inv_rim == 1u ? ndotv : (1.0 - ndotv);

  return albedo * rim.rgb * pow(rim_dot, rim_power);
}

vec3 calc_blinn_phong(vec3 position, vec3 normal, vec3 albedo, float metallic,
  float roughness, vec3 emissive, vec3 ao) {
  vec3 view_dir = normalize(-position);
  vec3 rim_color = calc_rim_light(view_dir, normal, albedo);

  vec3 color = ao * albedo + rim_color + emissive;
  float shininess = max((1.0 - roughness) * 256.0, 1.0);

  if (diff_int < EPSILON && spec_int < EPSILON)
    return color;

  // Directional lights
  for (uint i = 0; i < count; ++i) {
    vec3 light_dir = dir_lights[i].dir.xyz;
    vec3 albedo_light = albedo * dir_lights[i].color.rgb;

    // Diffuse
    if (diff_int >= EPSILON) {
      float ndotl = max(dot(normal, light_dir), 0.0);
      color += albedo_light * ndotl * diff_int;
    }

    // Specular
    if (spec_int >= EPSILON) {
      vec3 halfway = normalize(light_dir + view_dir);
      float ndoth = max(dot(normal, halfway), 0.0);
      float spec = pow(ndoth, shininess);
      color += albedo_light * spec * spec_int;
    }
  }

  return color;
}

// ============================================================================
// Main
// ============================================================================

void main() {
  // albedo
  vec4 albedo = material.color_factor;
  if (material.maps.x != -1)
    albedo *= texture(textures[material.maps.x], frag_uv);

  if (albedo.a < alpha_cutoff || albedo.a < EPSILON)
    return;

  // normal
  vec3 normal;
  if (material.maps.y != -1) {
    vec3 normal_map =
      texture(textures[material.maps.y], frag_uv).rgb * 2.0 - 1.0;
    normal = normalize(TBN * normal_map);
  } else {
    normal = normalize(TBN[2]);
  }

  if ((material.flags & MATERIAL_DOUBLE_SIDED) > 0 && !gl_FrontFacing)
    normal = -normal;

  // metallic-roughness
  float metallic = material.metallic_factor;
  float roughness = material.roughness_factor;

  if (material.maps.z != -1) {
    vec2 mr = texture(textures[material.maps.z], frag_uv).rg;
    metallic *= mr.r;
    roughness *= mr.g;
  }

  // emissive
  vec3 emissive = material.emissive_factor.rgb;
  if (material.maps.w != -1)
    emissive *= texture(textures[material.maps.w], frag_uv).rgb;

  // lighting
  vec3 lit_color;
  if (is_pbr == 1u)
    lit_color = calc_pbr(frag_pos, normal, albedo.rgb, metallic, roughness, emissive, ambient.rgb);
  else
    lit_color = calc_blinn_phong(frag_pos, normal, albedo.rgb, metallic, roughness, emissive, ambient.rgb);

  vec4 color = vec4(lit_color * albedo.a, albedo.a);
  float depth = gl_FragCoord.z;

  // insert into a-buffer
  uint coverage_mask = gl_SampleMaskIn[0];
  uint pixel_idx = (uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)) * SAMPLES;

  while (coverage_mask != 0u) {
    uint sample_id = findLSB(coverage_mask);
    coverage_mask &= ~(1u << sample_id);

    uint node_idx = atomicAdd(counter, 1);
    if (node_idx >= max_nodes) return; // out of memory

    nodes[node_idx].color = color;
    nodes[node_idx].depth = depth;
    nodes[node_idx].next = atomicExchange(heads[pixel_idx + sample_id], node_idx);
  }
}
