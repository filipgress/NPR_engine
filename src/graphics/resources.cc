#include "resources.h"

namespace npr_graphics {

Resources::Resources(const VulkanContext& context) : c_{context} {
  frame_props_.samples = std::min(vk::SampleCountFlagBits::e4, GetMaxSamples());
  frame_props_.depth_stencil_format = context.FindFormat(
      {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
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
