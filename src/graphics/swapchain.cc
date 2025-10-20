#include "swapchain.h"

namespace npr_graphics {

Swapchain::Swapchain(const VulkanContext& context, glm::ivec2 frame_size)
    : c_{context} {
  CreateSwapchain(frame_size);
  CreateImageViews();

  if (props_.is_HDR)
    INFO("color space: HDR(hlg)");
  else
    INFO("color space: SDR(nonlinear)");

  switch (props_.mode) {
    case vk::PresentModeKHR::eMailbox:
      INFO("present mode: Mailbox");
      break;
    case vk::PresentModeKHR::eFifoRelaxed:
      INFO("present mode: FifoRelaxed");
      break;
    default:
      INFO("present mode: Fifo");
  }

  INFO("swapchain images: ", props_.image_count);
}

Swapchain::~Swapchain() {
  DestroyImageViews();
  if (swapchain_) c_.GetDevice().destroySwapchainKHR(swapchain_);
}

void Swapchain::DestroyImageViews() {
  if (image_views_.empty()) return;

  for (const auto& image_view : image_views_)
    c_.GetDevice().destroyImageView(image_view);
  image_views_.clear();
}

void Swapchain::CreateSwapchain(glm::ivec2 frame_size,
                                vk::SwapchainKHR old_swapchain) {
  auto swap_supp = c_.GetSwapSupp();
  auto q_families = c_.GetQFamilies();

  auto [is_HDR, surface_format] = ChooseSurfaceFormat(swap_supp);
  props_.is_HDR = is_HDR;
  props_.format = surface_format.format;
  props_.color_space = surface_format.colorSpace;
  props_.mode = ChoosePresentMode(swap_supp);

  vk::SwapchainCreateInfoKHR swapchain_info{};
  swapchain_info.flags = vk::SwapchainCreateFlagsKHR();
  swapchain_info.surface = c_.GetSurface();
  swapchain_info.minImageCount = ChooseMinFrameCount(swap_supp);
  swapchain_info.imageFormat = props_.format;
  swapchain_info.imageColorSpace = props_.color_space;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  swapchain_info.oldSwapchain = old_swapchain;

  uint32_t queueFamilyIndices[]{q_families.graphics_i.value(),
                                q_families.present_i.value()};

  if (q_families.IsUnique()) {
    swapchain_info.imageSharingMode = vk::SharingMode::eConcurrent;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    swapchain_info.imageSharingMode = vk::SharingMode::eExclusive;
  }

  swapchain_info.preTransform = swap_supp.capabilities.currentTransform;
  swapchain_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  swapchain_info.presentMode = props_.mode;
  swapchain_info.clipped = VK_TRUE;

  props_.extent = ChooseExtent(swap_supp, frame_size);
  swapchain_info.imageExtent = props_.extent;

  swapchain_ = c_.GetDevice().createSwapchainKHR(swapchain_info);
}

void Swapchain::CreateImageViews() {
  DestroyImageViews();

  std::vector<vk::Image> swapchain_images =
      c_.GetDevice().getSwapchainImagesKHR(swapchain_);

  props_.image_count = swapchain_images.size();
  image_views_.resize(props_.image_count);

  for (size_t i = 0; i < swapchain_images.size(); ++i) {
    vk::ImageViewCreateInfo image_view_info{};
    image_view_info.flags = vk::ImageViewCreateFlags();
    image_view_info.image = swapchain_images[i];
    image_view_info.viewType = vk::ImageViewType::e2D;
    image_view_info.format = props_.format;
    image_view_info.components = {
        vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
        vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    image_view_info.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1,
                                        0, 1};

    image_views_[i] = c_.GetDevice().createImageView(image_view_info);
    c_.SetDbgName((uint64_t)(VkImageView)image_views_[i],
                  vk::ObjectType::eImageView,
                  "swap_image_view_" + std::to_string(i));
  }
}

uint32_t Swapchain::ChooseMinFrameCount(const SwapSupport& swap_supp,
                                        uint preferred) const {
  uint32_t desired = std::max(preferred, swap_supp.capabilities.minImageCount);
  if (swap_supp.capabilities.maxImageCount > 0)
    return std::min(desired, swap_supp.capabilities.maxImageCount);
  return desired;
}

vk::PresentModeKHR Swapchain::ChoosePresentMode(
    const SwapSupport& swap_supp) const {
  const auto& modes = swap_supp.present_modes;
  for (auto pref :
       {vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifoRelaxed})
    if (std::find(modes.begin(), modes.end(), pref) != modes.end()) return pref;

  // eFifo is guaranteed to be available
  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Swapchain::ChooseExtent(const SwapSupport& swap_supp,
                                     glm::ivec2 frame_size) const {
  if (swap_supp.capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max())
    return swap_supp.capabilities.currentExtent;

  return vk::Extent2D{std::clamp(static_cast<uint32_t>(frame_size.x),
                                 swap_supp.capabilities.minImageExtent.width,
                                 swap_supp.capabilities.maxImageExtent.width),
                      std::clamp(static_cast<uint32_t>(frame_size.y),
                                 swap_supp.capabilities.minImageExtent.height,
                                 swap_supp.capabilities.maxImageExtent.height)};
}

const std::set<vk::SurfaceFormatKHR> Swapchain::kHDRFormats{
    {vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eHdr10HlgEXT},
};
const std::set<vk::SurfaceFormatKHR> Swapchain::kSDRFormats{
    {vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
    {vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
};

std::pair<bool, vk::SurfaceFormatKHR> Swapchain::ChooseSurfaceFormat(
    const SwapSupport& swap_supp) const {
  const auto& available_formats = swap_supp.formats;
  if (available_formats.empty())
    throw std::runtime_error("No available surface formats!");

  for (const auto& format : available_formats)
    if (kHDRFormats.contains(format)) return {true, format};

  for (const auto& format : available_formats)
    if (kSDRFormats.contains(format)) return {false, format};

  ERR("Could not find supported surface format. Using first available!");
  return {false, available_formats.front()};
}

void Swapchain::Recreate(glm::ivec2 frame_size) {
  props_.dirty = false;

  DestroyImageViews();

  vk::SwapchainKHR old_swapchain = swapchain_;
  CreateSwapchain(frame_size, old_swapchain);
  CreateImageViews();

  if (old_swapchain) c_.GetDevice().destroySwapchainKHR(old_swapchain);
}

}  // namespace npr_graphics
