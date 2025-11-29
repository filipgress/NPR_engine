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

constexpr uint32_t kAONoiseDim = 4;
constexpr uint32_t kAOKernelSize = 64;

constexpr uint32_t kMaxGaussianRadius = 10;

using AOKernel = std::array<glm::vec4, kAOKernelSize>;

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
  glm::vec4 color;  // rgb = color * intensity, a = unused
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
  glm::vec4 color;  // rgb = color, a = unused
};

struct SpotLightUnif {
  glm::vec4 pos;  // xyz = view-space position, w = range
  glm::vec4 dir;  // xyz = normalized view-space direction to light, w = unused
  glm::vec4 color;   // rgb = color, a = unused
  glm::vec4 params;  // x = angle_scale, y = angle_offset, zw = unused
};

struct ABuffNode {
  glm::vec4 color;

  float depth;
  uint32_t next;
  uint32_t _padding[2];
};

struct AOPushConst {
  float radius;
  float bias;
};

struct LightPushConst {
  glm::mat4 model;
  float diff_int;
  float spec_int;
  uint32_t is_pbr;  // 0 = blinn-phong, 1 = pbr
};

struct ABuffFillPushConst {
  uint32_t width;
  uint32_t max_nodes;
  float alpha_cutoff;

  float diff_int;
  float spec_int;
  uint32_t is_pbr;  // 0 = blinn-phong, 1 = pbr
};

struct ABuffResPushConst {
  uint32_t width;
  uint32_t sorted_nodes;
};

struct WBoitPushConst {
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

struct BlurPushConst {
  glm::ivec4 flags;  // x = horizontal(1) / vertical(0), y = radius, zw = unused
  float weights[kMaxGaussianRadius + 1];  // weights[0] to weights[radius]
};

struct LoadPushConst {
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

  // ao
  const vk::Format ao_noise_format = vk::Format::eR16G16Sfloat;
  const vk::Format ao_format = vk::Format::eR16Unorm;

  // wboit layout
  const vk::Format acc_color_format = vk::Format::eR16G16B16A16Sfloat;
  const vk::Format acc_weight_format = vk::Format::eR16Sfloat;
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
  std::unique_ptr<Texture> emissive_roughness_ms;
  std::unique_ptr<Texture> position_ms;
  std::unique_ptr<Texture> normal_ms;

  std::unique_ptr<Image> coverage_ms;
  std::unique_ptr<Texture> coverage_res;

  std::unique_ptr<Texture> ds_ms;  // depth-stencil

  // ao
  std::unique_ptr<Image> ao_ms;
  std::unique_ptr<Texture> ao_res;
  std::unique_ptr<Texture> ao_temp;  // for separable blur

  // abuff
  std::unique_ptr<StorageBuffer<uint32_t>> abuff_heads;
  std::unique_ptr<StorageBuffer<ABuffNode>> abuff_nodes;
  std::unique_ptr<StorageBuffer<uint32_t>> abuff_counter;

  // wboit
  std::unique_ptr<Image> acc_color_ms;
  std::unique_ptr<Image> acc_color_res;

  std::unique_ptr<Image> acc_weight_ms;
  std::unique_ptr<Image> acc_weight_res;

  // color targets
  std::unique_ptr<Image> color_ms;
  std::unique_ptr<Texture> color_res;

  std::unique_ptr<Texture> temp_color;
  std::unique_ptr<Texture> bright_color;
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

  const LightMesh& GetSphereMesh() const { return *sphere_mesh_; }
  const LightMesh& GetConeMesh() const { return *cone_mesh_; }

  BlurPushConst& GetSSAOBlurPC() { return ssao_blur_; }
  const Texture& GetDefaultColorTex() const { return *default_color_tex_; }
  const Texture& GetAONoiseTex() const { return *ao_noise_tex_; }
  const UniformBuffer<AOKernel>& GetAOKernel() const { return *ao_kernel_; }

  const std::vector<FrameResources>& GetResrc() const { return frame_resrc_; }

 private:
  void CreateImages();
  void CreateBuffers();
  void CreateABuffers();

  void CreateAONoiseTex(vk::CommandBuffer cmd_buff);
  void CreateAOKernel();

  void CreateSphereMesh(vk::CommandBuffer cmd_buff);
  void CreateConeMesh(vk::CommandBuffer cmd_buff);

  void CreateDefaultColorTex(vk::CommandBuffer cmd_buff);
  BlurPushConst CreateGaussianKernel(uint32_t radius);

  vk::SampleCountFlagBits GetMaxSamples();

 private:
  const Context& ctx_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resrc_;
  uint frame_count_;

  std::unique_ptr<Texture> default_color_tex_;

  // ao resources
  std::unique_ptr<Texture> ao_noise_tex_;
  std::unique_ptr<UniformBuffer<AOKernel>> ao_kernel_;

  BlurPushConst ssao_blur_;

  // primitive meshes / light volumes
  std::unique_ptr<LightMesh> cone_mesh_;
  std::unique_ptr<LightMesh> sphere_mesh_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
