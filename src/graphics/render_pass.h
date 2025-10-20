#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include "vulkan_context.h"
#include "swapchain.h"

namespace npr_graphics {
class BasePass : public npr_core::NonCopyable {
 public:
  BasePass(const VulkanContext& context) : c_{context} {}
  virtual ~BasePass();

  vk::RenderPassBeginInfo BeginInfo(uint frame_idx, vk::Extent2D extent) const;
  vk::RenderPass GetRenderPass() const { return render_pass_; }

 protected:
  void Init() {
    SetClearValues();
    CreateRenderPass();
    CreateFramebuffers();
  }

  void CreateRenderPass();

  virtual void CreateFramebuffers() = 0;
  void DestroyFramebuffers();

  virtual void SetClearValues() = 0;
  virtual const std::string GetDbgName() const = 0;

  virtual std::vector<vk::AttachmentDescription> GetAttachments() const = 0;
  virtual std::vector<vk::SubpassDependency> GetDependencies() const = 0;
  virtual std::vector<vk::SubpassDescription> GetSubpasses() = 0;

 protected:
  const VulkanContext& c_;

  vk::RenderPass render_pass_{nullptr};
  std::vector<vk::Framebuffer> framebuffers_;
  std::vector<vk::ClearValue> clear_values_;
};

class SwapPass : public BasePass {
 public:
  SwapPass(const VulkanContext& context, const Swapchain& swapchain)
      : BasePass{context}, swapchain_{swapchain} {
    Init();
  }

  void Recreate() { CreateFramebuffers(); }

 private:
  void CreateFramebuffers() override;
  void SetClearValues() override;

  const std::string GetDbgName() const override { return "swap_pass"; }

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;

 private:
  const Swapchain& swapchain_;

  vk::AttachmentReference color_ref_{};
};

}  // namespace npr_graphics

#endif  // RENDER_PASS_H_
