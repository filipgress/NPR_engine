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
        bright_color_{ctx, resrc},
        bright_temp_{ctx, resrc},
        coc_map_{ctx, resrc},
        color_res_{ctx, resrc},
        present_color_{ctx, resrc},
        blend_present_{ctx, resrc},
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

    bright_color_.CreateFramebuffers();
    bright_temp_.CreateFramebuffers();

    coc_map_.CreateFramebuffers();

    color_res_.CreateFramebuffers();
    present_color_.CreateFramebuffers();
    blend_present_.CreateFramebuffers();
  }

 private:
  GPass gpass_;

  SSAOPass ssao_;
  SSAOTempPass ao_temp_;
  SSAOResPass ao_res_;

  GlobLightPass glob_light_;
  LocalLightPass local_light_;

  ABuffPass abuff_;
  WBoitPass wboit_;

  BrightColorPass bright_color_;
  BrightTempPass bright_temp_;

  CocMapPass coc_map_;

  ColorResPass color_res_;
  PresentColorPass present_color_;
  BlendPresentPass blend_present_;

  SwapPass swap_;
};
}  // namespace npr_graphics

#endif  // PASS_MANAGER_H_
