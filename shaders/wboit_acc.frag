#version 450

layout(constant_id = 0) const uint SAMPLES = 4;
layout(constant_id = 1) const uint MAX_TEXTURES = 128;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_pos;
layout(location = 2) in mat3 TBN;

layout(location = 0) out vec4 acc_color;
layout(location = 1) out float acc_alpha;

layout(set = 1, binding = 0) uniform sampler2D textures[MAX_TEXTURES];
layout(set = 2, binding = 0) uniform MaterialUniform {
  ivec4 maps;
  vec4 color_factor;
  vec3 emissive_factor;
  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
} material;

void main() {
  vec4 albedo = material.color_factor;
  if (material.maps.x != -1)
    albedo *= texture(textures[material.maps.x], frag_uv);

  if (albedo.a < 0.01) discard; // insignificant

  vec3 normal = normalize(TBN[2]);
  vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, light_dir), 0.0);
  vec3 lit_color = albedo.rgb * (0.3 + 0.7 * diff); // Ambient + diffuse

  // https://jcgt.org/published/0002/02/09/
  float w = clamp(pow(min(1.0, albedo.a * 10.0) + 0.01, 3.0) * 1e8 *
        pow(1.0 - gl_FragCoord.z * 0.9, 3.0),
      1e-2, 3e3);

  acc_color = vec4(lit_color * albedo.a, albedo.a) * w;
  acc_alpha = albedo.a * w;
}
