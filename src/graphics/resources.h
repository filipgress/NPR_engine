#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "vulkan_context.h"
#include "scene/scene.h"

namespace npr_graphics {

enum MaterialFlags : uint32_t {
  kNone = BIT(0),
  kDoubleSided = BIT(1),
  kOpaque = BIT(2),
  kMask = BIT(3)
};

struct alignas(16) MaterialUniform {
  glm::ivec4 maps;

  glm::vec4 color_factor;
  glm::vec3 emissive_factor;
  float mettalic_factor;
  float roughness_factor;

  float alpha_cutoff;
  MaterialFlags flags;
};

struct alignas(16) CameraUniform {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 view_proj;
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
  std::unique_ptr<UniformBuffer<CameraUniform>> camera_unif;
  std::unique_ptr<DynamicUniformBuffer<MaterialUniform>> material_unif;
  std::unique_ptr<Buffer> light_storage;
};

class Resources : public npr_core::NonCopyable {
 public:
  Resources(const VulkanContext& context);

  uint GetFrameCount() const { return frame_count_; }
  const std::vector<FrameResources>& GetResources() const {
    return frame_resources_;
  }

  // bind scenes that have finished loading for rendering
  // void Bind(npr_scene::Scene&& scene) { scene_ = std::move(scene); }

 private:
  vk::SampleCountFlagBits GetMaxSamples();

 private:
  const VulkanContext& c_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resources_;
  uint frame_count_;

  npr_scene::Scene scene_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
