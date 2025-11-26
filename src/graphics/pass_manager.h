#ifndef PASS_MANAGER_H_
#define PASS_MANAGER_H_

#include "render_pass.h"

namespace npr_graphics {
class PassManager : public npr_core::NonCopyable {
  friend class Renderer;
  friend class PipeManager;

 public:
  PassManager(const Context& ctx, const Swapchain& swapchain,
              const Resources& resrc)
      : gbuff_{ctx, resrc},
        ao_{ctx, resrc},
        ao_blur_h_{ctx, resrc},
        ao_blur_v_{ctx, resrc},
        glob_light_{ctx, resrc},
        local_light_{ctx, resrc},
        abuff_{ctx, resrc},
        wboit_{ctx, resrc},
        swap_{ctx, swapchain} {}
  ~PassManager() = default;

  void RecreateFramebuffers() {
    gbuff_.CreateFramebuffers();
    ao_.CreateFramebuffers();
    ao_blur_h_.CreateFramebuffers();
    ao_blur_v_.CreateFramebuffers();
    glob_light_.CreateFramebuffers();
    local_light_.CreateFramebuffers();
    abuff_.CreateFramebuffers();
    wboit_.CreateFramebuffers();
  }

 private:
  GBuffPass gbuff_;
  AOPass ao_;
  AOBlurHPass ao_blur_h_;
  AOBlurVPass ao_blur_v_;
  GlobLightPass glob_light_;
  LocalLightPass local_light_;
  ABuffPass abuff_;
  WBoitPass wboit_;
  SwapPass swap_;
};
}  // namespace npr_graphics

#endif  // PASS_MANAGER_H_
