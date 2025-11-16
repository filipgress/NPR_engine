#version 450

layout(constant_id = 0) const int MAX_GAUSSIAN_RADIUS = 10;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_blur;

layout(set = 0, binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform BlurPC {
  ivec4 flags; // dir, radius, (unused, unused)
  float weights[MAX_GAUSSIAN_RADIUS + 1];
};

void main() {
  vec2 texel = 1.0 / textureSize(input_tex, 0);
  vec2 dir = (flags.x == 0) ? vec2(1, 0) : vec2(0, 1);

  int radius = flags.y;
  vec4 res = vec4(0.0);

  for (int i = -radius; i <= radius; ++i)
    res += texture(input_tex, frag_uv + dir * i * texel) * weights[abs(i)];

  out_blur = res;
}
