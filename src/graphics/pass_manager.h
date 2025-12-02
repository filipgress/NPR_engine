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
      : gpass_{ctx, resrc},
        ssao_{ctx, resrc},
        ao_temp_{ctx, resrc},
        ao_res_{ctx, resrc},
        glob_light_{ctx, resrc},
        local_light_{ctx, resrc},
        abuff_{ctx, resrc},
        wboit_{ctx, resrc},
        bright_extract_{ctx, resrc},
        blur_bright_{ctx, resrc},
        coc_{ctx, resrc},
        dof_{ctx, resrc},
        post_{ctx, resrc},
        swap_{ctx, swapchain} {}
  ~PassManager() = default;

  void RecreateFramebuffers() {
    gpass_.CreateFramebuffers();

    ssao_.CreateFramebuffers();
    ao_temp_.CreateFramebuffers();
    ao_res_.CreateFramebuffers();

    glob_light_.CreateFramebuffers();
    local_light_.CreateFramebuffers();

    abuff_.CreateFramebuffers();
    wboit_.CreateFramebuffers();

    bright_extract_.CreateFramebuffers();
    blur_bright_.CreateFramebuffers();

    coc_.CreateFramebuffers();
    dof_.CreateFramebuffers();

    post_.CreateFramebuffers();
  }

 private:
  GPass gpass_;

  SSAOPass ssao_;
  AOTempPass ao_temp_;
  AOResPass ao_res_;

  GlobLightPass glob_light_;
  LocalLightPass local_light_;

  ABuffPass abuff_;
  WBoitPass wboit_;

  BrightPass bright_extract_;
  BlurBrightPass blur_bright_;

  CocPass coc_;
  DofPass dof_;

  PostPass post_;

  SwapPass swap_;
};
}  // namespace npr_graphics

#endif  // PASS_MANAGER_H_
