#version 450

layout(constant_id = 0) const int HATCH_LEVELS = 6;

const float EPSILON = 1e-5;

const uint DITHER_NONE = 0u;
const uint DITHER_WHITE_NOISE = 1u;
const uint DITHER_BAYER = 2u;
const uint DITHER_BLUE_NOISE = 3u;

const uint HATCH_NONE = 0u;
const uint HATCH_HATCH = 1u;
const uint HATCH_CROSS_HATCH = 2u;
const uint HATCH_SCRIBBLE = 3u;
const uint HATCH_STIPPLE = 4u;

const uint QUANT_NONE = 0u;
const uint QUANT_GRAYSCALE = 1u;
const uint QUANT_RGB = 2u;
const uint QUANT_PALETTE_LUMA = 3u;
const uint QUANT_PALETTE_NEAREST = 4u;

const uint BAYER_SIZE_2X2 = 2u;
const uint BAYER_SIZE_4X4 = 4u;
const uint BAYER_SIZE_8X8 = 8u;

const float CRT_MASK_BORDER = 0.9;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(set = 1, binding = 0) uniform sampler2D white_noise_tex;
layout(set = 1, binding = 1) uniform sampler2D blue_noise_tex;
layout(set = 2, binding = 0) uniform sampler2D palette_tex;
layout(set = 3, binding = 0) uniform sampler2DArray hatch_textures;

layout(push_constant) uniform PostPC {
  uint pixel_size;

  uint dither_mode; // 0 = none, 1 = white, 2 = bayer, 3 = blue
  uint bayer_size; // 2, 4, 8
  float dither_strength;

  uint quant_mode; // 0 = none, 1 = grayscale, 2 = rgb, 3 = palette, 4 = hue
  uint color_levels; // per channel for rgb, total for grayscale/palette

  uint hatch_mode; // 0 = none, 1 = hatch, 2 = cross-hatch, 3 = scribble, 4 = stipple
  float hatch_int;
  float hatch_density;

  uint enable_crt;
  float crt_curve_int;
  float crt_chroma;
  float crt_scanline_int;
  float crt_mask_int;
  float crt_distortion_speed;
  float crt_distortion_int;

  float t;
};

// ============================================================================
// BAYER MATRICES
// ============================================================================

const mat2 bayer_matrix_2x2 = mat2(
    0.0, 2.0,
    3.0, 1.0
  ) / 4.0;

const mat4 bayer_matrix_4x4 = mat4(
    0.0, 8.0, 2.0, 10.0,
    12.0, 4.0, 14.0, 6.0,
    3.0, 11.0, 1.0, 9.0,
    15.0, 7.0, 13.0, 5.0
  ) / 16.0;

const float bayer_matrix_8x8[64] = float[64](
    0.0 / 64.0, 48.0 / 64.0, 12.0 / 64.0, 60.0 / 64.0, 3.0 / 64.0, 51.0 / 64.0, 15.0 / 64.0, 63.0 / 64.0,
    32.0 / 64.0, 16.0 / 64.0, 44.0 / 64.0, 28.0 / 64.0, 35.0 / 64.0, 19.0 / 64.0, 47.0 / 64.0, 31.0 / 64.0,
    8.0 / 64.0, 56.0 / 64.0, 4.0 / 64.0, 52.0 / 64.0, 11.0 / 64.0, 59.0 / 64.0, 7.0 / 64.0, 55.0 / 64.0,
    40.0 / 64.0, 24.0 / 64.0, 36.0 / 64.0, 20.0 / 64.0, 43.0 / 64.0, 27.0 / 64.0, 39.0 / 64.0, 23.0 / 64.0,
    2.0 / 64.0, 50.0 / 64.0, 14.0 / 64.0, 62.0 / 64.0, 1.0 / 64.0, 49.0 / 64.0, 13.0 / 64.0, 61.0 / 64.0,
    34.0 / 64.0, 18.0 / 64.0, 46.0 / 64.0, 30.0 / 64.0, 33.0 / 64.0, 17.0 / 64.0, 45.0 / 64.0, 29.0 / 64.0,
    10.0 / 64.0, 58.0 / 64.0, 6.0 / 64.0, 54.0 / 64.0, 9.0 / 64.0, 57.0 / 64.0, 5.0 / 64.0, 53.0 / 64.0,
    42.0 / 64.0, 26.0 / 64.0, 38.0 / 64.0, 22.0 / 64.0, 41.0 / 64.0, 25.0 / 64.0, 37.0 / 64.0, 21.0 / 64.0
  );

// ============================================================================
// DITHERING FUNCTIONS
// ============================================================================

float sample_bayer_matrix(uint size) {
  ivec2 pixel = ivec2(gl_FragCoord.xy /
        vec2(pixel_size));

  if (size == BAYER_SIZE_2X2) {
    int x = pixel.x % 2;
    int y = pixel.y % 2;
    return bayer_matrix_2x2[y][x];
  }

  if (size == BAYER_SIZE_4X4) {
    int x = pixel.x % 4;
    int y = pixel.y % 4;
    return bayer_matrix_4x4[y][x];
  }

  // BAYER_SIZE_8X8
  int x = pixel.x % 8;
  int y = pixel.y % 8;
  return bayer_matrix_8x8[y * 8 + x];
}

float sample_white_noise() {
  vec2 noise_size = textureSize(white_noise_tex, 0);
  vec2 color_size = textureSize(color_tex, 0);

  vec2 noise_uv = frag_uv * color_size / (pixel_size * noise_size);
  return texture(white_noise_tex, noise_uv).r;
}

float sample_blue_noise() {
  vec2 noise_size = textureSize(blue_noise_tex, 0);
  vec2 color_size = textureSize(color_tex, 0);

  vec2 noise_uv = frag_uv * color_size / (pixel_size * noise_size);
  return texture(blue_noise_tex, noise_uv).r;
}

float get_dither_threshold() {
  if (dither_mode == DITHER_NONE)
    return 0.5; // no dither
  if (dither_mode == DITHER_WHITE_NOISE)
    return sample_white_noise();
  if (dither_mode == DITHER_BAYER)
    return sample_bayer_matrix(bayer_size);
  if (dither_mode == DITHER_BLUE_NOISE)
    return sample_blue_noise();

  return 0.5; // fallback
}

// ============================================================================
// COLOR QUANTIZATION
// ============================================================================

float get_lum(vec3 color) {
  return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 get_closest_color(vec3 color) {
  uint palette_size = textureSize(palette_tex, 0).x;

  if (palette_size == 0u)
    return color;

  vec3 closest = vec3(0.0);
  float min_distance = 1e10;

  for (int i = 0; i < palette_size; i++) {
    float u = (float(i) + 0.5) / float(palette_size);
    vec3 palette_color = texture(palette_tex, vec2(u, 0.5)).rgb;

    vec3 diff = color - palette_color;
    float distance = dot(diff, diff);

    if (distance < min_distance) {
      min_distance = distance;
      closest = palette_color;
    }
  }

  return closest;
}

vec3 quantize_palette_nearest(vec3 color, float threshold) {
  color += (threshold - 0.5) * dither_strength * 0.1;
  return get_closest_color(color);
}

vec3 quantize_palette_luma(vec3 color, float threshold) {
  float lum = get_lum(color) + (threshold - 0.5) * dither_strength * 0.1;
  return texture(palette_tex, vec2(lum, 0.5)).rgb;
}

vec3 quantize_grayscale(vec3 color, float threshold) {
  float lum = get_lum(color) + (threshold - 0.5) * dither_strength * 0.1;
  float quantized = floor(lum * (color_levels - 1.0) + 0.5) / (color_levels - 1.0);

  return vec3(quantized);
}

vec3 quantize_rgb(vec3 color, float threshold) {
  color += (threshold - 0.5) * dither_strength * 0.1;
  float divisor = color_levels - 1.0;

  color.r = floor(color.r * divisor + 0.5) / divisor;
  color.g = floor(color.g * divisor + 0.5) / divisor;
  color.b = floor(color.b * divisor + 0.5) / divisor;

  return color;
}

vec3 apply_quantization(vec3 color) {
  if (quant_mode == QUANT_NONE)
    return color;

  float threshold = get_dither_threshold();
  if (quant_mode == QUANT_GRAYSCALE)
    return quantize_grayscale(color, threshold);
  if (quant_mode == QUANT_RGB)
    return quantize_rgb(color, threshold);
  if (quant_mode == QUANT_PALETTE_LUMA)
    return quantize_palette_luma(color, threshold);
  if (quant_mode == QUANT_PALETTE_NEAREST)
    return quantize_palette_nearest(color, threshold);

  return color; // fallback
}

// ============================================================================
// HATCHING
// ============================================================================
vec3 apply_hatching(vec2 uv, vec3 color) {
  if (hatch_mode == HATCH_NONE) return color;

  float lum = get_lum(color);
  float level_f = (1.0 - lum) * float(HATCH_LEVELS);
  int level = int(clamp(level_f, 0.0, float(HATCH_LEVELS)));

  if (level == 0) return color;

  ivec3 hatch_size_3d = textureSize(hatch_textures, 0);
  vec2 hatch_size = hatch_size_3d.xy;
  vec2 hatch_uv = (uv * textureSize(color_tex, 0)) / (hatch_size / hatch_density);
  float hatch_value = texture(hatch_textures, vec3(hatch_uv, level - 1)).r;

  // vec2 tex_scale = textureSize(color_tex, 0) / textureSize(hatch_textures[0], 0);
  // float hatch_value = texture(hatch_textures[level], frag_uv * tex_scale).r;

  return color * mix(1.0, hatch_value, hatch_int);
}

// ============================================================================
// PIXELIZATION
// ============================================================================
vec2 apply_pixelization(vec2 uv) {
  if (pixel_size == 1u) return uv;

  vec2 pixel = vec2(pixel_size) / textureSize(color_tex, 0);
  return pixel * floor(uv / pixel);
}

// ============================================================================
// CRT EFFECT
// ============================================================================
vec2 apply_distortion(vec2 uv) {
  if (crt_distortion_int < EPSILON) return uv;

  vec2 noise_uv = vec2(uv.y * 3.0, t * crt_distortion_speed);
  float noise = texture(white_noise_tex, noise_uv).r;

  uv.x += (noise - 0.5) * crt_distortion_int;
  return uv;
}

vec2 apply_curve(vec2 uv) {
  if (crt_curve_int < EPSILON) return uv;

  vec2 curve_uv = uv * 2.0 - 1.0;
  vec2 offset = curve_uv.yx * crt_curve_int;
  curve_uv += curve_uv * offset * offset;
  uv = curve_uv * 0.5 + 0.5;

  return uv;
}

float get_screen_mask(vec2 uv) {
  vec2 edge = smoothstep(0.0, 0.02, uv) * (1.0 - smoothstep(0.98, 1.0, uv));
  return edge.x * edge.y;
}

vec3 apply_rgb_cell_mask(vec2 uv, vec3 color) {
  if (crt_mask_int < EPSILON) return color;

  vec2 pixel = uv * textureSize(color_tex, 0);
  vec2 coord = pixel / float(pixel_size);
  vec2 subcoord = coord * vec2(3.0, 1.0);

  vec2 cell_offset = vec2(0.0, mod(floor(coord.x), 3.0) * 0.5);

  // rgb subcell mask
  float ind = mod(floor(subcoord.x), 3.0);
  vec3 mask_color = vec3(ind == 0.0, ind == 1.0, ind == 2.0) * 2.0;

  // cell border mask
  vec2 cell_uv = fract(subcoord + cell_offset) * 2.0 - 1.0;
  vec2 border = 1.0 - cell_uv * cell_uv * CRT_MASK_BORDER;
  mask_color *= border.x * border.y;

  return color * (1.0 + (mask_color - 1.0) * crt_mask_int);
}

vec3 apply_scanlines(vec2 uv, vec3 color) {
  if (crt_scanline_int < EPSILON) return color;

  float lines = sin(uv.y * 2000.0 + t * 100.0);
  return color * (1.0 + lines * crt_scanline_int);
}

vec3 sample_with_chroma(vec2 uv) {
  if (crt_chroma < EPSILON) return texture(color_tex, uv).rgb;

  vec2 spread = vec2(crt_chroma * 1e-2);

  float r = texture(color_tex, uv + spread).r;
  float g = texture(color_tex, uv).g;
  float b = texture(color_tex, uv - spread).b;

  return vec3(r, g, b);
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
  vec2 uv = frag_uv;

  if (enable_crt == 0u) {
    uv = apply_pixelization(uv);
    vec3 color = texture(color_tex, uv).rgb;

    color = apply_quantization(color);
    color = apply_hatching(frag_uv, color);
    out_color = vec4(color, 1.0);

    return;
  }

  uv = apply_distortion(uv);
  vec2 curve_uv = apply_curve(uv);

  float screen_mask = get_screen_mask(curve_uv);
  if (screen_mask <= 0.0) {
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  vec2 pixel_uv = apply_pixelization(curve_uv);
  vec3 color = sample_with_chroma(pixel_uv);

  color = apply_quantization(color);
  color = apply_hatching(frag_uv, color);
  color = apply_rgb_cell_mask(frag_uv, color);
  color = apply_scanlines(curve_uv, color);

  color *= screen_mask;
  out_color = vec4(color, 1.0);
}
