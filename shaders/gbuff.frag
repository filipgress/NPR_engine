#version 450

layout(constant_id = 0) const int NUM_SAMPLES = 4;
layout(constant_id = 1) const int MAX_TEXTURES = 128;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_pos;
layout(location = 2) in mat3 TBN;

layout(location = 0) out vec4 out_albedo; // rgb = albedo, a = metallic
layout(location = 1) out vec4 out_emissive; // rgb = emissive, a = roughness
layout(location = 2) out vec4 out_position; // xyz = world position, w = unused
layout(location = 3) out vec4 out_normal; // xyz = world normal, w = unused
layout(location = 4) out float out_coverage; // Coverage for MSAA resolve

layout(set = 1, binding = 0) uniform sampler2D textures[MAX_TEXTURES];
layout(set = 2, binding = 0) uniform MaterialUniform {
  ivec4 maps; // x=albedo, y=normal, z=metallic_roughness, w=emissive

  vec4 color_factor;
  vec3 emissive_factor;
  float metallic_factor;
  float roughness_factor;

  float alpha_cutoff;
  uint flags;
} material;

uint coverage_mask = (1u << NUM_SAMPLES) - 1u;
const uint MATERIAL_DOUBLE_SIDED = 1 << 0;
const uint MATERIAL_MASK = 1 << 2;

void main() {
  // albedo
  vec4 albedo = material.color_factor;
  if (material.maps.x != -1)
    albedo *= texture(textures[material.maps.x], frag_uv);

  if ((material.flags & MATERIAL_MASK) != 0 && albedo.a < material.alpha_cutoff)
    discard;

  // normal
  vec3 normal;
  if (material.maps.y != -1) {
    vec3 normal_map =
      texture(textures[material.maps.y], frag_uv).rgb * 2.0f - 1.0f;
    normal = normalize(TBN * normal_map);
  } else {
    normal = TBN[2];
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
  vec3 emissive = material.emissive_factor;
  if (material.maps.w != -1)
    emissive *= texture(textures[material.maps.w], frag_uv).rgb;

  // output to gbuffer
  out_albedo = vec4(albedo.rgb, metallic);
  out_emissive = vec4(emissive, roughness);
  out_position = vec4(frag_pos, 0.0);
  out_normal = vec4(normal, 0.0);
  out_coverage = (gl_SampleMaskIn[0] == coverage_mask) ? 1.0 // simple pixel
    : 0.0; // complex pixel
}
