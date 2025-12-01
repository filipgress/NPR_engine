#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D image_tex;

layout(push_constant) uniform LoadPC {
  uvec2 res;
  vec3 t;
  bool is_loading;
};

const vec4 PINK = vec4(pow(vec3(255.0 / 255.0, 1.0 / 255.0, 251.0 / 255.0), vec3(2.2)), 1.0);
const vec4 BLUE = vec4(pow(vec3(2.0 / 255.0, 169.0 / 255.0, 234.0 / 255.0), vec3(2.2)), 1.0);
const vec4 YELLOW = vec4(pow(vec3(244.0 / 255.0, 244.0 / 255.0, 130.0 / 255.0), vec3(2.2)), 1.0);

const float RADIUS = 22.0;
const float OFFSET = RADIUS * 0.3;

void main() {
  vec3 tex_color = texture(image_tex, frag_uv).rgb;

  if (!is_loading) {
    out_color = vec4(tex_color, 1.0);
    return;
  }

  vec2 frag_coord = frag_uv * res;
  vec2 center = res - 45.0;

  vec2 posA = center + vec2(0.0, -t.x * OFFSET * 0.5);
  vec2 posB = center + vec2(t.y * OFFSET, t.x * OFFSET);
  vec2 posC = center + vec2(-t.x * OFFSET, t.z * OFFSET);

  float maskA = smoothstep(RADIUS, 0.0, distance(frag_coord, posA));
  float maskB = smoothstep(RADIUS, 0.0, distance(frag_coord, posB));
  float maskC = smoothstep(RADIUS, 0.0, distance(frag_coord, posC));

  vec4 color = maskA * PINK + maskB * YELLOW + maskC * BLUE;
  out_color = vec4(mix(tex_color, color.rgb, color.a), 1.0);
}
