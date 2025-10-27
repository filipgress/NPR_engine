#include "resources.h"

namespace npr_graphics {

Resources::Resources(const VulkanContext& context, vk::Extent2D extent,
                     uint frame_count)
    : c_{context}, frame_count_{frame_count} {
  frame_props_.extent = extent;
  frame_props_.samples = std::min(vk::SampleCountFlagBits::e4, GetMaxSamples());
  frame_props_.depth_stencil_format = context.FindFormat(
      {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);

  if (frame_props_.samples == vk::SampleCountFlagBits::e1)
    INFO("multisampling is not supported!");

  frame_resources_.resize(frame_count_);

  CreateImages();
  CreateBuffers();
}

void Resources::CreateImages() {
  uint idx{0};
  for (auto& res : frame_resources_) {
    // gpass
    res.albedo_metallic_ms = std::make_unique<Texture>(
        c_, frame_props_.albedo_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "albedo_metallic_ms_" + std::to_string(idx));

    res.emissive_roughness_ms = std::make_unique<Texture>(
        c_, frame_props_.emissive_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "emissive_roughness_ms_" + std::to_string(idx));

    res.position_ms = std::make_unique<Texture>(
        c_, frame_props_.position_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "position_ms" + std::to_string(idx));

    res.normal_ms = std::make_unique<Texture>(
        c_, frame_props_.normal_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "normal_ms" + std::to_string(idx));

    res.coverage_ms = std::make_unique<Image>(
        c_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eInputAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "coverage_ms" + std::to_string(idx));

    res.coverage_res = std::make_unique<Texture>(
        c_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "coverage_res" + std::to_string(idx));

    res.depth_stencil_ms = std::make_unique<Texture>(
        c_, frame_props_.depth_stencil_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
        frame_props_.samples, "depth_stencil_ms" + std::to_string(idx));

    idx++;
  }
}

void Resources::CreateBuffers() {
  for (auto& res : frame_resources_) {
    res.camera_unif =
        std::make_unique<UniformBuffer<CameraUnif>>(c_, "camera_unif_buff");
    res.material_unif = std::make_unique<DynamicUniformBuffer<MaterialUnif>>(
        c_, MAX_MATERIALS, "material_dynamic_unif_buff");
  }
}

vk::SampleCountFlagBits Resources::GetMaxSamples() {
  vk::PhysicalDeviceProperties props = c_.GetProperties();
  vk::SampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;

  if (counts & vk::SampleCountFlagBits::e64)
    return vk::SampleCountFlagBits::e64;
  if (counts & vk::SampleCountFlagBits::e32)
    return vk::SampleCountFlagBits::e32;
  if (counts & vk::SampleCountFlagBits::e16)
    return vk::SampleCountFlagBits::e16;
  if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
  if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
  if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;

  return vk::SampleCountFlagBits::e1;
}

}  // namespace npr_graphics
