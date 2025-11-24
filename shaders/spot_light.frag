#version 450

layout(constant_id = 0) const int SAMPLES = 4;

const float EPSILON = 0.001;
const float PI = 3.14159265359;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2DMS g_albedo_metallic;
layout(set = 1, binding = 1) uniform sampler2DMS g_emissive_roughness;
layout(set = 1, binding = 2) uniform sampler2DMS g_position;
layout(set = 1, binding = 3) uniform sampler2DMS g_normal;
layout(set = 1, binding = 4) uniform sampler2D g_coverage;

layout(set = 2, binding = 0) uniform PointLightUnif {
  vec4 pos; // xyz = view-space position, w = range
  vec4 color; // rgb = color * intensity, a = unused
};

vec3 calc_blinn_phong(int idx, ivec2 coord) {
  vec3 normal = texelFetch(g_normal, coord, idx).xyz;

  // early exit for invalid normals
  if (dot(normal, normal) < EPSILON)
    return vec3(0.0);

  vec3 albedo = texelFetch(g_albedo_metallic, coord, idx).rgb;
  vec4 er = texelFetch(g_emissive_roughness, coord, idx);
  float roughness = clamp(er.a, 0.05, 1.0);
  vec3 position = texelFetch(g_position, coord, idx).xyz;

  // Calculate light direction and distance
  vec3 light_vec = pos.xyz - position;
  float dist = length(light_vec);

  // Early exit if fragment is outside light radius
  if (dist > pos.w)
    return vec3(0.0);

  vec3 light_dir = normalize(light_vec);
  vec3 view_dir = normalize(-position);

  // Calculate attenuation
  float attenuation = 1.0 - smoothstep(0.0, pos.w, dist);
  vec3 attenuated_color = color.rgb * attenuation;

  // Diffuse
  float ndotl = max(dot(normal, light_dir), 0.0);
  vec3 diffuse = albedo * attenuated_color * ndotl;

  // Specular
  vec3 specular = vec3(0.0);
  if (ndotl > 0.0) {
    vec3 halfway = normalize(light_dir + view_dir);
    float ndoth = max(dot(normal, halfway), 0.0);
    float shininess = (1.0 - roughness) * 256.0;
    float spec = pow(ndoth, shininess);
    specular = attenuated_color * spec;
  }

  return diffuse + specular;
}

void main() {
  out_color = vec4(0.0, 1.0, 0.0, 1.0);
  return;

  ivec2 coord = ivec2(gl_FragCoord.xy);
  vec2 frag_uv = coord / textureSize(g_coverage, 0);

  float coverage = texture(g_coverage, frag_uv).r;
  if (coverage == 1.0) { // simple pixel
    out_color = vec4(calc_blinn_phong(0, coord), 1.0);
    return;
  }

  // complex pixel
  vec3 final_color = vec3(0.0);
  for (int s = 0; s < SAMPLES; ++s)
    final_color += calc_blinn_phong(s, coord);

  out_color = vec4(final_color / float(SAMPLES), 1.0);
}
