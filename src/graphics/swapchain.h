#ifndef SWAPCHAIN_H_
#define SWAPCHAIN_H_

#include "context.h"

namespace npr_graphics {
struct SwapProps {
  vk::PresentModeKHR mode;
  vk::Extent2D extent;
  vk::Format format;
  vk::ColorSpaceKHR color_space;

  uint min_image_count;
  uint image_count;

  bool is_HDR{false};
  bool dirty{false};
};

class Swapchain : public npr_core::NonCopyable {
 public:
  Swapchain(const Context& ctx, glm::ivec2 frame_size);
  ~Swapchain();

  void Recreate(glm::ivec2 frame_size);

  vk::SwapchainKHR GetSwapchain() const { return swapchain_; }
  SwapProps GetProps() const { return props_; }
  SwapProps& GetProps() { return props_; }
  const auto& GetViews() const { return image_views_; }

 private:
  void CreateSwapchain(glm::ivec2 frame_size,
                       vk::SwapchainKHR old_swapchain = nullptr);

  void CreateImageViews();
  void DestroyImageViews();

  uint32_t ChooseMinFrameCount(const SwapSupport& swap_supp,
                               uint preferred = 3) const;
  vk::PresentModeKHR ChoosePresentMode(const SwapSupport& swap_supp) const;
  vk::Extent2D ChooseExtent(const SwapSupport& swap_supp,
                            glm::ivec2 frame_size) const;
  std::pair<bool, vk::SurfaceFormatKHR> ChooseSurfaceFormat(
      const SwapSupport& swap_supp) const;

 private:
  const Context& ctx_;

  vk::SwapchainKHR swapchain_{nullptr};
  SwapProps props_{};

  std::vector<vk::ImageView> image_views_;

  static const std::set<vk::SurfaceFormatKHR> kSDRFormats;
  static const std::set<vk::SurfaceFormatKHR> kHDRFormats;
};
}  // namespace npr_graphics

#endif  // SWAPCHAIN_H_
