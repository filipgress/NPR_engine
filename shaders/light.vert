#version 450

layout(location = 0) in vec3 in_pos;

layout(set = 0, binding = 0) uniform CameraUnif {
  mat4 view;
  mat4 proj;
  mat4 proj_view;
};

layout(push_constant) uniform LightPC {
  mat4 model;
};

void main() {
  gl_Position = proj_view * model * vec4(in_pos, 1.0);
}
