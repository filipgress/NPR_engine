#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "vulkan_context.h"
#include "command_pool.h"
#include "image.h"

#include "scene/components.h"

namespace npr_graphics {

#define MAX_TEXTURES 128
#define MAX_MATERIALS 1024
#define MAX_INSTANCES 65536  // 2^16

#define MAX_DIR_LIGHTS 2
#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

#define ABUFF_INIT_SIZE 8  // avg fragments per pixel

#define AO_NOISE_DIM 4
#define AO_KERNEL_SIZE 64
using AOKernel = std::array<glm::vec4, AO_KERNEL_SIZE>;

#define MAX_GAUSSIAN_RADIUS 10

enum MaterialFlags : uint32_t {
  kNone = BIT(0),
  kDoubleSided = BIT(1),
  kOpaque = BIT(2),
  kMask = BIT(3)
};

struct alignas(16) MaterialUnif {
  glm::ivec4 maps;

  glm::vec4 color_factor;
  glm::vec3 emissive_factor;
  float metallic_factor;
  float roughness_factor;

  float alpha_cutoff;
  uint32_t flags;
};

struct alignas(16) CameraUnif {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 proj_view;
};

struct DirLightStorage {
  glm::vec4 direction;  // xyz = normalized direction, w = unused
  glm::vec4 color;      // rgb = color, a = intensity
};

struct PointLightStorage {
  glm::vec4 position;  // xyz = position, w = radius
  glm::vec4 color;     // rgb = color, a = intensity
};

struct SpotLightStorage {
  glm::vec4 position;   // xyz = position, w = radius
  glm::vec4 direction;  // xyz = direction, w = unused
  glm::vec4 color;      // rgb = color, a = intensity
  glm::vec4 params;     // x = angle_scale, y = angle_offset, zw = unused
};

struct LightStorage {
  glm::vec4 ambient_color;
  glm::ivec4 counts;  // xyz = dir, point & spot count, w = unused

  DirLightStorage dir_lights[MAX_DIR_LIGHTS];
  PointLightStorage point_lights[MAX_POINT_LIGHTS];
  SpotLightStorage spot_lights[MAX_SPOT_LIGHTS];
};

struct alignas(16) FragmentNode {
  glm::vec4 color;
  float depth;
  uint32_t next;
};

struct alignas(16) ABuffFillPushConst {
  uint32_t width{0};
  uint32_t max_nodes;

  uint32_t _padding[2];
};

struct alignas(16) ABuffResolvePushConst {
  uint32_t width{0};
  uint32_t max_sorted_nodes{4};

  uint32_t _padding[2];
};

struct alignas(16) BlurPushConst {
  glm::ivec4 flags;  // x = horizontal(1) / vertical(0), y = radius, zw = unused
  float weights[MAX_GAUSSIAN_RADIUS + 1];  // weights[0] to weights[radius]
};

struct LoadPushConst {
  glm::uvec2 res{0};
  alignas(16) glm::vec3 t{0.0f};
  bool is_loading{false};
};

struct FrameProps {
  vk::SampleCountFlagBits samples;
  vk::Extent2D extent;

  // for HDR use vk::Format::eR16G16B16A16Sfloat
  vk::Format color_format = vk::Format::eR8G8B8A8Srgb;
  vk::Format depth_stencil_format;

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
  const vk::Format ao_format = vk::Format::eR8Unorm;

  // wboit layout
  const vk::Format acc_color_format = vk::Format::eR16G16B16A16Sfloat;
  const vk::Format acc_weight_format = vk::Format::eR16Sfloat;
};

struct FrameResources {
  std::unique_ptr<InstanceBuffer> instance_buff;
  std::unique_ptr<UniformBuffer<CameraUnif>> camera_ubo;
  std::unique_ptr<DynamicUniformBuffer<MaterialUnif>> material_ubo;
  std::unique_ptr<StorageBuffer<LightStorage>> light_storage;

  // gpass
  std::unique_ptr<Texture> albedo_metallic_ms;
  std::unique_ptr<Texture> emissive_roughness_ms;
  std::unique_ptr<Texture> position_ms;
  std::unique_ptr<Texture> normal_ms;

  std::unique_ptr<Image> coverage_ms;
  std::unique_ptr<Texture> coverage_res;

  std::unique_ptr<Texture> depth_stencil_ms;

  // ao
  std::unique_ptr<Image> ao_ms;
  std::unique_ptr<Texture> ao_res;
  std::unique_ptr<Texture> ao_temp;  // for separable blur

  // abuff transparency
  std::unique_ptr<Buffer> abuff_heads;
  std::unique_ptr<Buffer> abuff_nodes;
  std::unique_ptr<Buffer> abuff_counter;

  // wboit
  std::unique_ptr<Image> acc_color_ms;
  std::unique_ptr<Image> acc_color_res;

  std::unique_ptr<Image> acc_weight_ms;
  std::unique_ptr<Image> acc_weight_res;

  // swap pass
  std::unique_ptr<Texture> present_color;
};

struct Mesh {
  VertexBuffer vbo;
  IndexBuffer ibo;
  npr_scene::BoundingBoxComp bb;

  Mesh(VertexBuffer&& v, IndexBuffer&& i, const npr_scene::BoundingBoxComp& b)
      : vbo(std::move(v)), ibo(std::move(i)), bb{b} {}
};

class Resources : public npr_core::NonCopyable {
  friend class Renderer;

 public:
  Resources(const VulkanContext& context, const CommandPool& cmd_pool,
            vk::Extent2D extent, uint frame_count);

  uint GetFrameCount() const { return frame_count_; }
  const FrameProps& GetProps() const { return frame_props_; }
  BlurPushConst& GetSSAOBlurPC() { return ssao_blur_; }
  const Texture& GetDefaultColorTex() const { return *default_color_tex_; }
  const Texture& GetAONoiseTex() const { return *ao_noise_tex_; }
  const UniformBuffer<AOKernel>& GetAOKernel() const { return *ao_kernel_; }

  const std::vector<FrameResources>& GetResources() const {
    return frame_resources_;
  }

 private:
  void CreateImages();
  void CreateBuffers();
  void CreateABuffers();
  BlurPushConst CreateGaussianKernel(int radius);

  void CreateAONoiseTex(vk::CommandBuffer cmd_buff);
  void CreateAOKernel();

  void CreateSphereMesh(vk::CommandBuffer cmd_buff);
  void CreateConeMesh(vk::CommandBuffer cmd_buff);

  void CreateDefaultColorTex(vk::CommandBuffer cmd_buff);

  vk::SampleCountFlagBits GetMaxSamples();

 private:
  const VulkanContext& c_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resources_;
  uint frame_count_;

  std::unique_ptr<Texture> default_color_tex_;

  // ao resources
  std::unique_ptr<Texture> ao_noise_tex_;
  std::unique_ptr<UniformBuffer<AOKernel>> ao_kernel_;

  BlurPushConst ssao_blur_;

  // primitive meshes / light volumes
  std::unique_ptr<Mesh> cone_mesh_;
  std::unique_ptr<Mesh> sphere_mesh_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
