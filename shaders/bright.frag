#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_bright;

layout(set = 0, binding = 0) uniform sampler2D color_tex;

layout(push_constant) uniform PushConst {
  float threshold;
  float soft_threshold;
  float intensity;
};

void main() {
  vec3 color = texture(color_tex, frag_uv).rgb;

  float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

  float knee = threshold * soft_threshold;
  float soft = brightness - threshold + knee;
  soft = clamp(soft, 0.0, 2.0 * knee);
  soft = (soft * soft) / (4.0 * knee + 1e-4);

  float contribution = max(soft, brightness - threshold);
  contribution = max(0.0, contribution);

  out_bright = vec4(color * contribution / max(brightness, 1e-4) * intensity, 1.0);
}
