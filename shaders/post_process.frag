#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(set = 1, binding = 0) uniform sampler2D white_noise_tex;
layout(set = 1, binding = 1) uniform sampler2D blue_noise_tex;
layout(set = 2, binding = 0) uniform sampler2D palette_tex;

layout(push_constant) uniform PostPC {
  uint pixel_size;

  uint dither_mode; // 0 = none, 1 = white, 2 = bayer, 3 = blue
  uint bayer_size; // 2, 4, 8
  float dither_strength;

  uint quant_mode; // 0 = none, 1 = grayscale, 2 = rgb, 3 = palette, 4 = hue
  uint color_levels; // per channel for rgb, total for grayscale/palette
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

float get_bayer_threshold(uint size) {
  ivec2 pixel = ivec2(gl_FragCoord.xy /
        vec2(pixel_size));

  if (size == 2u) {
    int x = pixel.x % 2;
    int y = pixel.y % 2;
    return bayer_matrix_2x2[y][x];
  } else if (size == 4u) {
    int x = pixel.x % 4;
    int y = pixel.y % 4;
    return bayer_matrix_4x4[y][x];
  } else {
    int x = pixel.x % 8;
    int y = pixel.y % 8;
    return bayer_matrix_8x8[y * 8 + x];
  }
}

float get_white_noise_tex() {
  vec2 noise_uv = frag_uv * textureSize(color_tex, 0) / (pixel_size * textureSize(white_noise_tex, 0));
  return texture(white_noise_tex, noise_uv).r;
}

float get_blue_noise_tex() {
  vec2 noise_uv = frag_uv * textureSize(color_tex, 0) / (pixel_size * textureSize(blue_noise_tex, 0));
  return texture(blue_noise_tex, noise_uv).r;
}

float get_dither_threshold() {
  if (dither_mode == 1u)
    return get_white_noise_tex();
  else if (dither_mode == 2u)
    return get_bayer_threshold(bayer_size);
  else if (dither_mode == 3u)
    return get_blue_noise_tex();

  return 0.5; // no dither
}

// ============================================================================
// COLOR QUANTIZATION
// ============================================================================

vec3 find_closest_palette_color(vec3 color) {
  int palette_size = textureSize(palette_tex, 0).x;

  vec3 closest = vec3(0.0);
  float min_distance = 999999.0;

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
  color = clamp(color, 0.0, 0.99);

  return find_closest_palette_color(color);
}

vec3 quantize_grayscale(vec3 color, float threshold) {
  float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
  lum += (threshold - 0.5) * dither_strength * 0.1;

  float quantized = floor(lum * (color_levels - 1.0) + 0.5) / (color_levels - 1.0);

  return vec3(quantized);
}

vec3 quantize_rgb(vec3 color, float threshold) {
  vec3 adjusted = color + (threshold - 0.5) * dither_strength * 0.1;

  adjusted.r = floor(adjusted.r * (color_levels - 1.0) + 0.5) / (color_levels - 1.0);
  adjusted.g = floor(adjusted.g * (color_levels - 1.0) + 0.5) / (color_levels - 1.0);
  adjusted.b = floor(adjusted.b * (color_levels - 1.0) + 0.5) / (color_levels - 1.0);

  return clamp(adjusted, 0.0, 1.0);
}

vec3 quantize_palette_luma(vec3 color, float threshold) {
  float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
  lum += (threshold - 0.5) * dither_strength * 0.1;
  lum = clamp(lum, 0.0, 0.99);

  return texture(palette_tex, vec2(lum, 0.5)).rgb;
}

vec3 apply_quantization(vec3 color, float threshold) {
  if (quant_mode == 0u) {
    return color;
  } else if (quant_mode == 1u) {
    return quantize_grayscale(color, threshold);
  } else if (quant_mode == 2u) {
    return quantize_rgb(color, threshold);
  } else if (quant_mode == 3u) {
    return quantize_palette_luma(color, threshold);
  } else {
    return quantize_palette_nearest(color, threshold);
  }
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
  vec2 uv = frag_uv;

  // === PIXELIZATION ===
  if (pixel_size > 1.0) {
    vec2 pixel = vec2(pixel_size) / textureSize(color_tex, 0);
    uv = pixel * floor(frag_uv / pixel);
  }

  // sample color
  vec4 color = texture(color_tex, uv);

  // === DITHERING & QUANTIZATION ===
  float threshold = get_dither_threshold();
  color.rgb = apply_quantization(color.rgb, threshold);

  out_color = color;
}
