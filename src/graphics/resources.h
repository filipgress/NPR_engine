#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "vulkan_context.h"
#include "command_pool.h"
#include "image.h"

namespace npr_graphics {

#define MAX_TEXTURES 128
#define MAX_MATERIALS 1024
#define MAX_INSTANCES 16384

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
  glm::mat4 view_proj;
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
};

struct FrameResources {
  std::unique_ptr<InstanceBuffer> instance_buff;
  std::unique_ptr<UniformBuffer<CameraUnif>> camera_ubo;
  std::unique_ptr<DynamicUniformBuffer<MaterialUnif>> material_ubo;
  // std::unique_ptr<Buffer> light_storage;

  // gpass
  std::unique_ptr<Texture> albedo_metallic_ms;
  std::unique_ptr<Texture> emissive_roughness_ms;
  std::unique_ptr<Texture> position_ms;
  std::unique_ptr<Texture> normal_ms;

  std::unique_ptr<Image> coverage_ms;
  std::unique_ptr<Texture> coverage_res;

  std::unique_ptr<Texture> depth_stencil_ms;

  // swap pass
  std::unique_ptr<Texture> present_color;
};

class Resources : public npr_core::NonCopyable {
  friend class Renderer;

 public:
  Resources(const VulkanContext& context, const CommandPool& cmd_pool,
            vk::Extent2D extent, uint frame_count);

  uint GetFrameCount() const { return frame_count_; }
  const FrameProps& GetProps() const { return frame_props_; }
  const Texture& GetDefaultTexture() const { return *default_tex_; }
  const std::vector<FrameResources>& GetResources() const {
    return frame_resources_;
  }

 private:
  void CreateImages();
  void CreateBuffers();
  void CreateDefaultTexture(vk::CommandBuffer cmd_buff);

  vk::SampleCountFlagBits GetMaxSamples();

 private:
  const VulkanContext& c_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resources_;
  uint frame_count_;

  std::unique_ptr<Texture> default_tex_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
