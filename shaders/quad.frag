#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 4) uniform sampler2D image_tex;

void main() {
    out_color = texture(image_tex, frag_uv);
}
