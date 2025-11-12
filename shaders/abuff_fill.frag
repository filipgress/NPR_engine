#version 450

layout(constant_id = 0) const uint SAMPLES = 4;
layout(constant_id = 1) const uint MAX_TEXTURES = 128;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_pos;
layout(location = 2) in mat3 TBN;

struct FragmentNode {
  float depth;
  vec4 color;
  uint next;
  uint _padding[3];
};

layout(set = 1, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

layout(set = 2, binding = 0) buffer FragmentNodes {
  FragmentNode nodes[];
};
layout(set = 2, binding = 1) buffer HeadPointers {
  uint heads[];
};
layout(set = 2, binding = 2) buffer NodeCounter {
  uint counter;
};

layout(set = 3, binding = 0) uniform MaterialUniform {
  ivec4 maps;
  vec4 color_factor;
  vec3 emissive_factor;
  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
} material;

layout(push_constant) uniform PushConst {
  uint width;
  uint max_nodes;
};

void main() {
  vec4 albedo = material.color_factor;
  if (material.maps.x != -1)
    albedo *= texture(textures[material.maps.x], frag_uv);

  if (albedo.a < 0.01) discard; // insignificant

  uint coverage_mask = gl_SampleMaskIn[0];
  uint pixel_idx = (uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)) * SAMPLES;

  vec4 color = vec4(albedo.rgb * albedo.a, albedo.a);
  float depth = gl_FragCoord.z;

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
