#version 450

layout(constant_id = 0) const int SAMPLES = 4;
layout(constant_id = 1) const int MAX_DIR_LIGHTS = 2;

const float EPSILON = 0.001;

const uint SHADING_GOOCH = 2u;
const uint SHADING_TOON = 3u;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out float out_int;

layout(set = 0, binding = 0) uniform sampler2D ao_tex;

layout(set = 1, binding = 0) uniform sampler2DMS g_albedo_metallic;
layout(set = 1, binding = 1) uniform sampler2DMS g_emissive_roughness;
layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;
layout(set = 1, binding = 4) uniform sampler2D g_coverage;

struct DirLight {
  vec4 dir; // xyz = normalized view-space direction to light, w = unused
  vec4 color; // rgb = color * intensity, a = intensity
};

layout(set = 2, binding = 0) uniform DirLightUnif {
  vec4 ambient; // rgb = color * intensity, a = intensity
  vec4 rim; // rgb = color * intensity, a = intensity

  float rim_power;
  uint inv_rim; // 0 = normal, 1 = inverse
  uint count;
  uint use_ssao;

  DirLight dir_lights[MAX_DIR_LIGHTS];
};

layout(push_constant) uniform LightPC {
  mat4 model; // unused

  uint shading_mode; // 2 = gooch, 3 = toon
  float diff_int;
  float spec_int;
};

float calc_rim_intensity(vec3 view_dir, vec3 normal) {
  if (rim.a < EPSILON) return 0.0;

  float ndotv = max(dot(view_dir, normal), 0.0);
  float rim_dot = inv_rim == 1u ? ndotv : (1.0 - ndotv);

  return pow(rim_dot, rim_power) * rim.a;
}

float calc_light_intensity(int idx, ivec2 coord, float ao_int) {
  vec3 normal = texelFetch(g_normal, coord, idx).xyz;

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return ao_int;

  vec4 er = texelFetch(g_emissive_roughness, coord, idx);
  vec3 position = texelFetch(g_position, coord, idx).xyz;

  vec3 emissive = er.rgb;
  float roughness = clamp(er.a, 0.05, 1.0);
  vec3 view_dir = normalize(-position);

  float rim_int = calc_rim_intensity(view_dir, normal);
  float emissive_int = dot(emissive, vec3(0.299, 0.587, 0.114));

  // ao + rim + emissive
  float total_int = ao_int + rim_int + emissive_int;
  float shininess = max((1.0 - roughness) * 256.0, 1.0);

  for (uint i = 0; i < count; ++i) {
    vec3 light_dir = dir_lights[i].dir.xyz;

    // diffuse
    if (diff_int > EPSILON) {
      float ndotl = dot(normal, light_dir);
      float t;
      if (shading_mode == SHADING_GOOCH)
        t = (ndotl + 1.0) * 0.5;
      else // SHADING_TOON
        t = max(ndotl, 0.0);
      total_int += t * dir_lights[i].color.a * diff_int;
    }

    // specular
    if (spec_int > EPSILON) {
      vec3 halfway = normalize(light_dir + view_dir);
      float ndoth = max(dot(normal, halfway), 0.0);
      float spec = pow(ndoth, shininess);
      total_int += spec * dir_lights[i].color.a * spec_int; // * 0.1;
    }
  }

  return total_int;
}

void main()
{
  ivec2 coord = ivec2(gl_FragCoord.xy);

  float ao_int = ambient.a;
  if (ao_int >= EPSILON && use_ssao != 0u)
    ao_int *= texture(ao_tex, frag_uv).r;

  float coverage = texture(g_coverage, frag_uv).r;

  if (coverage == 1.0) { // simple pixel
    out_int = calc_light_intensity(0, coord, ao_int);
    return;
  }

  // complex pixel
  float total_int = 0.0;
  for (int s = 0; s < SAMPLES; ++s)
    total_int += calc_light_intensity(s, coord, ao_int);

  out_int = total_int / float(SAMPLES);
}
