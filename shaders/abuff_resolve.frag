#version 450

layout(constant_id = 0) const uint SAMPLES = 4;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_bright;

layout(set = 0, binding = 0) uniform sampler2D opaque_color;

struct ABuffNode {
  vec4 color;

  float depth;
  uint next;
  uint _padding[2];
};

layout(set = 1, binding = 0) buffer ABuffNodes {
  ABuffNode nodes[];
};

layout(set = 1, binding = 1) buffer HeadPointers {
  uint heads[];
};

layout(push_constant) uniform PushConst {
  uint width;
};

const uint NULL_PTR = 0xFFFFFFFF;
const uint MAX_FRAGMENTS = 16; // fragments to sort per pixel

void main() {
  uint pixel_idx = (uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)) * SAMPLES;
  vec4 acc_color = vec4(0.0);

  for (uint sample_id = 0; sample_id < SAMPLES; ++sample_id) {
    uint node_idx = heads[pixel_idx + sample_id];

    float depths[MAX_FRAGMENTS];
    vec4 colors[MAX_FRAGMENTS];
    int count = 0;

    // collect fragments for this sample
    while (node_idx != NULL_PTR && count < MAX_FRAGMENTS) {
      ABuffNode node = nodes[node_idx];
      depths[count] = node.depth;
      colors[count] = node.color;
      count++;
      node_idx = node.next;
    }

    if (count == 0) continue;

    // insertion sort by depth (front-to-back)
    for (int i = 1; i < count; ++i) {
      float key_depth = depths[i];
      vec4 key_color = colors[i];
      int j = i - 1;

      while (j >= 0 && depths[j] > key_depth) {
        depths[j + 1] = depths[j];
        colors[j + 1] = colors[j];
        j--;
      }

      depths[j + 1] = key_depth;
      colors[j + 1] = key_color;
    }

    vec4 dest = vec4(0.0);
    for (int i = 0; i < count; ++i) {
      vec4 src = colors[i];
      dest.rgb += src.rgb * (1.0 - dest.a);
      dest.a += src.a * (1.0 - dest.a);

      if (dest.a >= 0.99) break;
    }

    acc_color += dest;
  }

  vec4 transparent = acc_color / float(SAMPLES);
  vec4 opaque = texture(opaque_color, frag_uv);

  vec3 blended = mix(opaque.rgb, transparent.rgb, transparent.a);
  out_color = vec4(blended, 1.0);

  float brightness = dot(blended, vec3(0.2126, 0.7152, 0.0722));
  float threshold = 0.2;
  out_bright = brightness > threshold ? out_color : vec4(0.0);
}
