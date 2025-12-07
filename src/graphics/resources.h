#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "settings.h"
#include "context.h"
#include "command_pool.h"
#include "image.h"

namespace npr_graphics {

constexpr uint32_t kMaxTextures = 128;
constexpr uint32_t kMaxMaterials = 1024;
constexpr uint32_t kMaxInstances = 65536;  // 2^16

constexpr uint32_t kMaxDirLights = 3;
constexpr uint32_t kMaxPointLights = 4;
constexpr uint32_t kMaxSpotLights = 4;

constexpr uint32_t kABuffMaxSortedNodes = 16;
constexpr uint32_t kSSAONoiseDim = 4;
constexpr uint32_t kWhiteNoiseDim = 64;

constexpr uint32_t kSSAOKernelSize = 64;
constexpr uint32_t kMaxPoisSize = 128;
constexpr uint32_t kMaxGausRadius = 16;

constexpr uint kHatchLevels = 6;

using SSAOKernel = std::array<glm::vec4, kSSAOKernelSize>;

struct PoisKernelUnif {
  glm::uvec4 count;                 // x = count, yzw = unused
  glm::vec4 samples[kMaxPoisSize];  // xy = offset, zw = unused
};

struct GausKernelPC {
  glm::ivec4 flags;                   // x = dir, y = radius, zw = unused
  float weights[kMaxGausRadius + 1];  // weights[0] to weights[radius]
};

enum MaterialFlags : uint32_t {
  kNone = BIT(0),
  kDoubleSided = BIT(1),
  kOpaque = BIT(2),
  kMask = BIT(3)
};

struct MaterialUnif {
  glm::ivec4 maps;  // x=albedo, y=normal, z=metallic_roughness, w=emissive

  glm::vec4 color_factor;     // rgb = albedo, a = alpha
  glm::vec4 emissive_factor;  // rgb = emissive, a = unused

  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint32_t flags;
};

struct CameraUnif {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 proj_view;
};

struct DirLight {
  glm::vec4 dir;  // xyz = normalized view-space direction to light, w = unused
  glm::vec4 color;  // rgb = color * intensity, a = intnensity
};

struct DirLightUnif {
  glm::vec4 ambient;  // rgb = color * intensity, a = intensity
  glm::vec4 rim;      // rgb = color * intensity, a = intensity

  float rim_power;
  uint32_t inv_rim;  // 0 = normal, 1 = inverse
  uint32_t count;
  uint32_t use_ssao;

  DirLight dir_lights[kMaxDirLights];
};

struct PointLightUnif {
  glm::vec4 pos;    // xyz = view-space position, w = range
  glm::vec4 color;  // rgb = color, a = intensity
};

struct SpotLightUnif {
  glm::vec4 pos;  // xyz = view-space position, w = range
  glm::vec4 dir;  // xyz = normalized view-space direction to light, w = unused
  glm::vec4 color;   // rgb = color, a = intensity
  glm::vec4 params;  // x = angle_scale, y = angle_offset, zw = unused
};

struct ABuffNode {
  glm::vec4 color;

  float depth;
  uint32_t next;
  uint32_t _padding[2];
};

struct SSAOPC {
  float radius;
  float bias;
};

struct LightPC {
  glm::mat4 model;

  uint32_t shading_mode;  // 0 = blinn-phong, 1 = pbr
  float diff_int;
  float spec_int;
};

struct ShadingPC {
  glm::vec4 gooch_warm;
  glm::vec4 gooch_cool;

  uint32_t shading_mode;  // 0 = blinn-phong, 1 = pbr
  float gooch_alpha;
  float gooch_beta;

  uint32_t toon_steps;
  float toon_min_brightness;
  float toon_threshold;
};

struct ABuffFillPC {
  uint32_t width;
  uint32_t max_nodes;
  float alpha_cutoff;

  float diff_int;
  float spec_int;
  uint32_t is_pbr;  // 0 = blinn-phong, 1 = pbr
};

struct ABuffResPC {
  uint32_t width;
  uint32_t sorted_nodes;
};

struct WBoitPC {
  float alpha_multiplier{10.f};
  float alpha_power{3.0};
  float depth_factor{0.9};
  float depth_power{3.0};
  float weight_min{1e-2};
  float weight_max{3e3};

  float alpha_cutoff;
  float diff_int;
  float spec_int;
  uint32_t is_pbr;  // 0 = blinn-phong, 1 = pbr
};

struct BrightPC {
  float threshold;
  float soft_threshold;
  float intensity;
};

struct CocPC {
  float focus_dist;
  float focus_range;

  float near_int;
  float far_int;

  float near_falloff;
  float far_falloff;

  float near_plane;
  float far_plane;

  uint32_t is_persp;  // 1 = persp, 0 = ortho
};

struct DofPC {
  float blur_radius;
  float coc_threshold;
  float coc_falloff;
  uint32_t debug_mode;
};

struct PostProcessPC {
  uint32_t pixel_size;

  uint32_t dither_mode;  // 0 = none, 1 = white, 2 = bayer, 3 = blue
  uint32_t bayer_size;   // 2, 4, 8
  float dither_strength;

  uint32_t quant_mode;  // 0=none, 1 = grayscale, 2 = rgb, 3 = palette, 4 = hue
  uint32_t color_levels;  // per channel for rgb, total for grayscale/palette

  uint32_t hatch_mode;  // 0=none, 1=hatch, 2=cross-hatch, 3=scribble, 4=stipple
  float hatch_int;
  float hatch_density;

  uint32_t enable_crt;
  float crt_curve_int;
  float crt_chroma;

  float crt_scanline_int;
  float crt_mask_int;

  float crt_distortion_speed;
  float crt_distortion_int;

  float t;
};

struct LoadPC {
  glm::uvec2 res{0};
  alignas(16) glm::vec3 t{0.0f};
  bool is_loading{false};
};

struct FrameProps {
  vk::SampleCountFlagBits samples;
  vk::Extent2D extent;

  uint32_t abuff_avg_nodes{};  // average count of nodes per pixel
  uint32_t abuff_max_nodes{};

  vk::Format color_format = vk::Format::eR16G16B16A16Sfloat;
  vk::Format ds_format;

  // gbuffer layout
  // albedo: xyz = albedo, w = metallic
  // emissive: xyz = emissive, w = roughness
  const vk::Format albedo_format = vk::Format::eR8G8B8A8Unorm;
  const vk::Format emissive_format = vk::Format::eR8G8B8A8Unorm;
  const vk::Format position_format = vk::Format::eR16G16B16A16Sfloat;
  const vk::Format normal_format = vk::Format::eR16G16B16A16Sfloat;
  const vk::Format coverage_format = vk::Format::eR16Sfloat;
  const vk::Format light_map_format = vk::Format::eR16Sfloat;

  // ssao
  const vk::Format ssao_noise_format = vk::Format::eR16G16Sfloat;
  const vk::Format ssao_format = vk::Format::eR16Unorm;

  // bloom
  const vk::Format coc_format = vk::Format::eR16Sfloat;

  // wboit layout
  const vk::Format acc_color_format = vk::Format::eR16G16B16A16Sfloat;
  const vk::Format acc_weight_format = vk::Format::eR16Sfloat;

  // noise textures
  const vk::Format white_noise_format = vk::Format::eR8Unorm;
  const vk::Format blue_noise_format = vk::Format::eR8G8B8A8Unorm;
  const vk::Format hatch_format = vk::Format::eR8Unorm;
  const vk::Format palette_format = vk::Format::eR8G8B8A8Unorm;
};

struct FrameResources {
  std::unique_ptr<InstanceBuffer> instance_buff;
  std::unique_ptr<UniformBuffer<CameraUnif>> camera_ubo;
  std::unique_ptr<DynamicUniformBuffer<MaterialUnif>> material_ubo;

  std::unique_ptr<UniformBuffer<DirLightUnif>> dir_light_ubo;
  std::unique_ptr<DynamicUniformBuffer<PointLightUnif>> point_light_ubo;
  std::unique_ptr<DynamicUniformBuffer<SpotLightUnif>> spot_light_ubo;

  // gpass
  std::unique_ptr<Texture> albedo_metallic_ms;
  std::unique_ptr<Texture> albedo_metallic_res;

  std::unique_ptr<Texture> emissive_roughness_ms;
  std::unique_ptr<Texture> position_ms;
  std::unique_ptr<Texture> normal_ms;

  std::unique_ptr<Image> coverage_ms;
  std::unique_ptr<Texture> coverage_res;

  std::unique_ptr<Texture> ds_ms;  // depth-stencil

  // ssao
  std::unique_ptr<Image> ssao_ms;
  std::unique_ptr<Texture> ssao_res;
  std::unique_ptr<Texture> ssao_temp;  // for separable blur

  // lighting
  std::unique_ptr<Image> color_ms;
  std::unique_ptr<Texture> color_res;

  std::unique_ptr<Image> light_map_ms;
  std::unique_ptr<Texture> light_map_res;

  // abuff
  std::unique_ptr<StorageBuffer<uint32_t>> abuff_heads;
  std::unique_ptr<StorageBuffer<ABuffNode>> abuff_nodes;
  std::unique_ptr<StorageBuffer<uint32_t>> abuff_counter;

  // wboit
  std::unique_ptr<Image> acc_color_ms;
  std::unique_ptr<Image> acc_color_res;

  std::unique_ptr<Image> acc_weight_ms;
  std::unique_ptr<Image> acc_weight_res;

  // bloom
  std::unique_ptr<Texture> bright_color;
  std::unique_ptr<Texture> bright_temp;

  // dof
  std::unique_ptr<Texture> coc_map;

  // final
  std::unique_ptr<Texture> present_color;
};

struct LightMesh {
  VertexBuffer<LightVertex> vbo;
  IndexBuffer ibo;

  LightMesh(VertexBuffer<LightVertex>&& v, IndexBuffer&& i)
      : vbo(std::move(v)), ibo(std::move(i)) {}
};

class Resources : public npr_core::NonCopyable {
  friend class Renderer;

 public:
  Resources(const Context& ctx, const CommandPool& cmd_pool,
            const RenderSettings& settings, uint frame_count);

  uint GetFrameCount() const { return frame_count_; }
  const FrameProps& GetProps() const { return frame_props_; }
  const auto& GetResrc() const { return frame_resrc_; }

  const auto& GetSSAOKernel() const { return *ssao_kernel_; }
  const auto& GetPoisKernel32() const { return *pois_kernel_32_; }
  const auto& GetPoisKernel64() const { return *pois_kernel_64_; }
  const auto& GetPoisKernel128() const { return *pois_kernel_128_; }

  GausKernelPC& GetBlurSSAOPC() { return blur_ssao_pc_; }
  GausKernelPC& GetBlurBloomPC() { return blur_bloom_pc_; }

  const Texture& GetDefColorTex() const { return *default_color_tex_; }
  const Texture& GetWhiteNoiseTex() const { return *white_noise_tex_; }
  const Texture& GetSSAONoiseTex() const { return *ssao_noise_tex_; }

  const Texture& GetBlueNoiseTex64() const { return *blue_noise_tex_64_; }
  const Texture& GetBlueNoiseTex128() const { return *blue_noise_tex_128_; }
  const Texture& GetBlueNoiseTex256() const { return *blue_noise_tex_256_; }

  const Texture& GetBlueNoiseTex64_1() const { return *blue_noise_tex_64_1; }
  const Texture& GetBlueNoiseTex64_2() const { return *blue_noise_tex_64_2; }
  const Texture& GetBlueNoiseTex64_3() const { return *blue_noise_tex_64_3; }
  const Texture& GetBlueNoiseTex128_4() const { return *blue_noise_tex_128_4; }

  uint32_t GetPaletteCount() const { return palette_texs_.size(); }
  const Texture& GetPaletteTex(uint32_t idx) const {
    return *palette_texs_[idx];
  }

  uint32_t GetHatchTexCount() const { return hatch_texs_.size(); }
  const Texture& GetHatchTex(uint32_t idx) const { return *hatch_texs_[idx]; }

  uint32_t GetCrossHatchTexCount() const { return cross_hatch_texs_.size(); }
  const Texture& GetCrossHatchTex(uint32_t idx) const {
    return *cross_hatch_texs_[idx];
  }

  uint32_t GetScribbleTexCount() const { return scribble_texs_.size(); }
  const Texture& GetScribbleTex(uint32_t idx) const {
    return *scribble_texs_[idx];
  }

  uint32_t GetStippleTexCount() const { return stipple_texs_.size(); }
  const Texture& GetStippleTex(uint32_t idx) const {
    return *stipple_texs_[idx];
  }

  const LightMesh& GetSphereMesh() const { return *sphere_mesh_; }
  const LightMesh& GetConeMesh() const { return *cone_mesh_; }

 private:
  void CreateImages();
  void CreateBuffers();
  void CreateABuffers();

  void CreateSSAOKernel();
  void CreatePoisKernels();

  void CreateDefaultColorTex(vk::CommandBuffer cmd_buff);
  void CreateWhiteNoiseTex(vk::CommandBuffer cmd_buff);
  void CreateSSAONoiseTex(vk::CommandBuffer cmd_buff);
  void LoadBlueTextures(vk::CommandBuffer cmd_buff);
  void LoadPatternTextures(vk::CommandBuffer cmd_buff);

  void CreatePalettes(vk::CommandBuffer cmd_buff);

  void CreateSphereMesh(vk::CommandBuffer cmd_buff);
  void CreateConeMesh(vk::CommandBuffer cmd_buff);

  // helpers
  GausKernelPC GenGausKernel(uint32_t radius);
  PoisKernelUnif GenPoisKernel(uint32_t sample_count, uint32_t max_attempts);

  void LoadTex(vk::CommandBuffer cmd_buff, vk::Format format,
               const std::string& filepath,
               std::unique_ptr<Texture>& out_texture, const std::string& name);
  void CreatePalette(vk::CommandBuffer cmd_buff, const std::string& name,
                     const std::vector<glm::vec4>& colors);

  vk::SampleCountFlagBits GetMaxSamples();

 private:
  const Context& ctx_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resrc_;
  uint frame_count_;

  std::unique_ptr<UniformBuffer<SSAOKernel>> ssao_kernel_;
  std::unique_ptr<UniformBuffer<PoisKernelUnif>> pois_kernel_32_;
  std::unique_ptr<UniformBuffer<PoisKernelUnif>> pois_kernel_64_;
  std::unique_ptr<UniformBuffer<PoisKernelUnif>> pois_kernel_128_;

  GausKernelPC blur_ssao_pc_;
  GausKernelPC blur_bloom_pc_;

  std::unique_ptr<Texture> default_color_tex_;
  std::unique_ptr<Texture> white_noise_tex_;
  std::unique_ptr<Texture> ssao_noise_tex_;

  std::unique_ptr<Texture> blue_noise_tex_64_;
  std::unique_ptr<Texture> blue_noise_tex_128_;
  std::unique_ptr<Texture> blue_noise_tex_256_;

  std::unique_ptr<Texture> blue_noise_tex_64_1;
  std::unique_ptr<Texture> blue_noise_tex_64_2;
  std::unique_ptr<Texture> blue_noise_tex_64_3;
  std::unique_ptr<Texture> blue_noise_tex_128_4;

  std::vector<std::unique_ptr<Texture>> hatch_texs_;
  std::vector<std::unique_ptr<Texture>> cross_hatch_texs_;
  std::vector<std::unique_ptr<Texture>> scribble_texs_;
  std::vector<std::unique_ptr<Texture>> stipple_texs_;

  std::vector<std::unique_ptr<Texture>> palette_texs_;

  std::unique_ptr<LightMesh> cone_mesh_;
  std::unique_ptr<LightMesh> sphere_mesh_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
