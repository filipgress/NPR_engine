#version 450

layout(constant_id = 0) const int NOISE_DIM = 4;
layout(constant_id = 1) const int KERNEL_SIZE = 64;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out float out_ao;

layout(set = 0, binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
  mat4 proj_view;
};

layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;

layout(set = 2, binding = 0) uniform ssao_kernel {
  vec3 samples[KERNEL_SIZE];
};
layout(set = 2, binding = 1) uniform sampler2D noise_tex;

layout(push_constant) uniform PushConst {
  float radius;
  float bias;
};

void main() {
  ivec2 screen_size = textureSize(g_position);

  vec3 frag_pos = texelFetch(g_position, ivec2(gl_FragCoord.xy), 0).xyz;
  vec3 normal = texelFetch(g_normal, ivec2(gl_FragCoord.xy), 0).xyz;

  ivec2 noise_scale = screen_size / NOISE_DIM;
  vec3 random_vec = vec3(texture(noise_tex, frag_uv * noise_scale).xy, 0.0);

  vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 TBN = mat3(tangent, bitangent, normal);

  float occlusion = 0.0;
  for (int i = 0; i < KERNEL_SIZE; ++i) {
    vec3 sample_pos = TBN * samples[i];
    sample_pos = frag_pos + sample_pos * radius;

    vec4 sample_uv = proj * vec4(sample_pos, 1.0);
    sample_uv.xyz /= sample_uv.w;
    sample_uv.xyz = sample_uv.xyz * 0.5 + 0.5;

    float sample_depth =
      texelFetch(g_position, ivec2(sample_uv.xy * screen_size), 0).z;

    float range_check =
      smoothstep(0.0, 1.0, radius / abs(frag_pos.z - sample_depth));
    occlusion +=
      ((sample_depth >= sample_pos.z + bias) ? 1.0 : 0.0) * range_check;
  }

  out_ao = 1.0 - (occlusion / KERNEL_SIZE);
}
