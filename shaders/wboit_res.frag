#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_bright;

layout(set = 0, binding = 0) uniform sampler2D opaque_color;

layout(input_attachment_index = 0, set = 1,
binding = 0) uniform subpassInput acc_color;
layout(input_attachment_index = 1, set = 1,
binding = 1) uniform subpassInput acc_weight;

void main() {
  vec4 opaque = texture(opaque_color, frag_uv);

  vec4 acc = subpassLoad(acc_color);
  float rev = subpassLoad(acc_weight).r;

  vec3 transparent = acc.rgb / max(acc.a, 1e-5);
  float alpha = 1.0 - clamp(rev, 0.0, 1.0);

  vec3 blended = mix(opaque.rgb, transparent, alpha);
  out_color = vec4(blended, 1.0);

  float brightness = dot(blended, vec3(0.2126, 0.7152, 0.0722));
  float threshold = 0.2;
  out_bright = brightness > threshold ? out_color : vec4(0.0);
}
