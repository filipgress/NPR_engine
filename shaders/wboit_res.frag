#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(input_attachment_index = 0, set = 0,
binding = 0) uniform subpassInput acc_color;
layout(input_attachment_index = 1, set = 0,
binding = 1) uniform subpassInput acc_weight;

void main() {
  vec4 acc = subpassLoad(acc_color);
  float rev = subpassLoad(acc_weight).r;

  if (rev < 1e-5) discard;

  vec3 avg_color = acc.rgb / acc.a;
  float final_alpha = 1.0 - clamp(rev, 0.0, 1.0);

  out_color = vec4(avg_color, final_alpha);
}
