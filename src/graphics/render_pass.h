#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include "context.h"
#include "swapchain.h"
#include "resources.h"

namespace npr_graphics {
class BasePass : public npr_core::NonCopyable {
 public:
  BasePass(const Context& ctx) : ctx_{ctx} {}
  virtual ~BasePass();

  vk::RenderPassBeginInfo BeginInfo(uint frame_idx, vk::Extent2D extent) const;
  vk::RenderPass GetRenderPass() const { return render_pass_; }

  virtual void CreateFramebuffers() = 0;

 protected:
  void Init() {
    SetClearValues();
    CreateRenderPass();
    CreateFramebuffers();
  }

  void CreateRenderPass();
  void DestroyFramebuffers();

  virtual void SetClearValues() = 0;
  virtual const std::string GetDbgName() const = 0;

  virtual std::vector<vk::AttachmentDescription> GetAttachments() const = 0;
  virtual std::vector<vk::SubpassDependency> GetDependencies() const = 0;
  virtual std::vector<vk::SubpassDescription> GetSubpasses() = 0;

 protected:
  const Context& ctx_;

  vk::RenderPass render_pass_{nullptr};
  std::vector<vk::Framebuffer> framebuffers_;
  std::vector<vk::ClearValue> clear_values_;
};

class RenderPass : public BasePass {
 public:
  RenderPass(const Context& ctx, const Resources& resrc)
      : BasePass{ctx}, resrc_{resrc} {}
  virtual ~RenderPass() = default;

  void CreateFramebuffers() override;

 protected:
  virtual std::vector<vk::ImageView> GetAttachmentViews(
      int frame_idx) const = 0;

 protected:
  const Resources& resrc_;
};

class GBuffPass : public RenderPass {
 public:
  GBuffPass(const Context& ctx, const Resources& resrc)
      : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "gbuffer_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  std::array<vk::AttachmentReference, 5> color_refs_{};
  std::array<vk::AttachmentReference, 5> resolve_refs_{};
  vk::AttachmentReference depth_ref_{};
};

class AOPass : public RenderPass {
 public:
  AOPass(const Context& ctx, const Resources& resrc) : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "ao_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  vk::AttachmentReference ao_ms_ref_{};
  vk::AttachmentReference ao_res_ref_{};
  vk::AttachmentReference depth_ref_{};
};

class SingleColorPass : public RenderPass {
 public:
  SingleColorPass(const Context& ctx, const Resources& resrc, vk::Format format)
      : RenderPass(ctx, resrc), format_{format} {}

 private:
  void SetClearValues() override;
  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;

 private:
  vk::Format format_;
  vk::AttachmentReference color_ref_{};
};

class AOBlurHPass : public SingleColorPass {
 public:
  AOBlurHPass(const Context& ctx, const Resources& resrc)
      : SingleColorPass(ctx, resrc, resrc.GetProps().ao_format) {
    Init();
  }

 private:
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;
  const std::string GetDbgName() const override { return "ao_blur_h_pass"; }
};

class AOBlurVPass : public SingleColorPass {
 public:
  AOBlurVPass(const Context& ctx, const Resources& resrc)
      : SingleColorPass(ctx, resrc, resrc.GetProps().ao_format) {
    Init();
  }

 private:
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;
  const std::string GetDbgName() const override { return "ao_blur_v_pass"; }
};

class BrightPass : public SingleColorPass {
 public:
  BrightPass(const Context& ctx, const Resources& resrc)
      : SingleColorPass(ctx, resrc, resrc.GetProps().color_format) {
    Init();
  }

 private:
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;
  const std::string GetDbgName() const override {
    return "extract_bright_pass";
  }
};

class BlurHPass : public SingleColorPass {
 public:
  BlurHPass(const Context& ctx, const Resources& resrc)
      : SingleColorPass(ctx, resrc, resrc.GetProps().color_format) {
    Init();
  }

 private:
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;
  const std::string GetDbgName() const override { return "blur_h_pass"; }
};

class GlobLightPass : public RenderPass {
 public:
  GlobLightPass(const Context& ctx, const Resources& resrc)
      : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "glob_light_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  vk::AttachmentReference color_ref_{};
  vk::AttachmentReference ds_ref_{};
};

class LocalLightPass : public RenderPass {
 public:
  LocalLightPass(const Context& ctx, const Resources& resrc)
      : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "local_light_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  vk::AttachmentReference color_ref_{};
  vk::AttachmentReference ds_ref_{};
};

class ABuffPass : public RenderPass {
 public:
  ABuffPass(const Context& ctx, const Resources& resrc)
      : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "abuff_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  vk::AttachmentReference color_ref_{};
  vk::AttachmentReference depth_ref_{};
};

class WBoitPass : public RenderPass {
 public:
  WBoitPass(const Context& ctx, const Resources& resrc)
      : RenderPass{ctx, resrc} {
    Init();
  }

 private:
  const std::string GetDbgName() const override { return "wboit_pass"; }
  void SetClearValues() override;

  std::vector<vk::AttachmentDescription> GetAttachments() const override;
  std::vector<vk::SubpassDependency> GetDependencies() const override;
  std::vector<vk::SubpassDescription> GetSubpasses() override;
  std::vector<vk::ImageView> GetAttachmentViews(int frame_idx) const override;

 private:
  std::array<vk::AttachmentReference, 2> acc_refs_{};
  std::array<vk::AttachmentReference, 2> resolve_refs_{};
  vk::AttachmentReference depth_ref_{};

  vk::AttachmentReference color_ref_{};
  std::array<vk::AttachmentReference, 2> input_refs_{};
};

class SwapPass : public BasePass {
 public:
  SwapPass(const Context& ctx, const Swapchain& swapchain)
      : BasePass{ctx}, swapchain_{swapchain} {
    Init();
  }

  void CreateFramebuffers() override;

 private:
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
