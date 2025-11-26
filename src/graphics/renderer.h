#ifndef RENDERER_H_
#define RENDERER_H_

#include "context.h"
#include "swapchain.h"
#include "command_pool.h"
#include "descriptor_pool.h"
#include "sync.h"
#include "resources.h"
#include "render_pass.h"
#include "gui_manager.h"
#include "shader.h"
#include "pipeline.h"
#include "pipeline_cache.h"

#include "core/frame_timer.h"
#include "window/window.h"
#include "scene/scene.h"
#include "scene/camera.h"

namespace npr_graphics {

enum class TransparencyMode { kNone, kABuff, kWBoit };
struct RenderSettings {
  vk::Extent2D target_size{500, 400};
  glm::vec3 ambient_color{0.3f, 0.3f, 0.3f};
  float ambient_intensity{0.225f};

  bool enable_ssao{true};
  float ssao_radius{0.5f};
  float ssao_bias{0.025f};

  TransparencyMode trans_mode{TransparencyMode::kABuff};
};

class Renderer : public npr_core::NonCopyable {
 public:
  Renderer(const npr_window::Window& window) : window_{window} {}
  ~Renderer() { Finish(); }

  void Render(npr_core::FrameTimer& timer, npr_scene::Camera& camera,
              npr_scene::Scene& scene, bool is_loading);

  const Context& GetContext() const { return ctx_; }
  const Resources& GetResrc() const { return resrc_; }

  void Resize() { swapchain_.GetProps().dirty = true; }
  void RecompileShaders();
  void SwapShaders();

  void Finish() { ctx_.GetDevice().waitIdle(); }

 private:
  vk::CommandBuffer Record(uint image_idx, const npr_scene::Camera& camera,
                           npr_scene::Scene& scene, bool is_loading, float dt);

  // record opaque primitives
  void RecordGBuff(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const npr_graphics::FrameResources& frame_resrc,
                   const vk::Extent2D& resrc_extent,
                   const npr_scene::Camera& camera, npr_scene::Scene& scene);
  void RecordOpaque(vk::CommandBuffer cmd_buff, const uint frame_idx,
                    const npr_graphics::FrameResources& frame_resrc,
                    const npr_scene::Scene& scene,
                    const npr_scene::Frustum& frustum) const;

  // record light
  void RecordAO(vk::CommandBuffer cmd_buff, const uint frame_idx,
                const vk::Extent2D& resrc_extent);
  void RecordGlobLight(vk::CommandBuffer cmd_buff, const uint frame_idx,
                       const npr_graphics::FrameResources& frame_resrc,
                       const vk::Extent2D& resrc_extent,
                       const CameraUnif& cam_ubo, npr_scene::Scene& scene);
  void RecordLocalLight(vk::CommandBuffer cmd_buff, const uint frame_idx,
                        const npr_graphics::FrameResources& frame_resrc,
                        const vk::Extent2D& resrc_extent,
                        const CameraUnif& cam_ubo,
                        const npr_scene::Camera& camera,
                        npr_scene::Scene& scene);
  void RecordPointLights(vk::CommandBuffer cmd_buff, const uint frame_idx,
                         const npr_graphics::FrameResources& frame_resrc,
                         const vk::Extent2D& resrc_extent,
                         const CameraUnif& cam_ubo,
                         const npr_scene::Camera& camera,
                         npr_scene::Scene& scene);
  void RecordSpotLights(vk::CommandBuffer cmd_buff, const uint frame_idx,
                        const npr_graphics::FrameResources& frame_resrc,
                        const vk::Extent2D& resrc_extent,
                        const CameraUnif& cam_ubo,
                        const npr_scene::Camera& camera,
                        npr_scene::Scene& scene);

  // record transparent primitives
  void RecordABuff(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const npr_graphics::FrameResources& frame_resrc,
                   const vk::Extent2D& resrc_extent,
                   const npr_scene::Camera& camera, npr_scene::Scene& scene);
  void RecordWBoit(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const npr_graphics::FrameResources& frame_resrc,
                   const vk::Extent2D& resrc_extent,
                   const npr_scene::Camera& camera, npr_scene::Scene& scene);
  void RecordTrans(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const npr_graphics::FrameResources& frame_resrc,
                   const npr_scene::Scene& scene,
                   const npr_scene::Frustum& frustum, vk::PipelineLayout layout,
                   const uint set_idx) const;

  // final pass to swapchain
  void RecordSwap(vk::CommandBuffer cmd_buff, uint image_idx,
                  const uint frame_idx, float camera_aspect, bool is_loading,
                  float dt);

  void RenderTargetResize();
  std::pair<vk::Viewport, vk::Rect2D> CalcViewportScissor(
      vk::Extent2D swap_extent, float camera_aspect) const;

 private:
  static uint mat_at;   // cycling through material descriptor sets
  static uint inst_at;  // cycling through instance descriptor sets

  RenderSettings settings_;

  const npr_window::Window& window_;

  Context ctx_{window_};
  Swapchain swapchain_{ctx_, window_.GetSize()};

  Sync sync_{ctx_, swapchain_.GetProps().image_count};
  GraphicsCommandPool cmd_pool_{ctx_, sync_.GetFrameCount(), "renderer"};

  Resources resrc_{ctx_, cmd_pool_, settings_.target_size,
                   sync_.GetFrameCount()};
  DescriptorPool desc_pool_{ctx_, resrc_};

  // render passes
  GBuffPass gbuff_pass_{ctx_, resrc_};

  AOPass ao_pass_{ctx_, resrc_};
  AOBlurHPass ao_blur_h_pass_{ctx_, resrc_};
  AOBlurVPass ao_blur_v_pass_{ctx_, resrc_};

  GlobLightPass glob_light_pass_{ctx_, resrc_};
  LocalLightPass local_light_pass_{ctx_, resrc_};

  ABuffPass abuff_pass_{ctx_, resrc_};
  WBoitPass wboit_pass_{ctx_, resrc_};

  SwapPass swap_pass_{ctx_, swapchain_};

  // shaders
  std::array<VertexShader, 3> vert_shaders_{
      VertexShader{ctx_, "shaders/quad_vert.spv", "../shaders/quad.vert"},
      VertexShader{ctx_, "shaders/gbuff_vert.spv", "../shaders/gbuff.vert"},
      VertexShader{ctx_, "shaders/light_vert.spv", "../shaders/light.vert"}};

  std::array<FragmentShader, 11> frag_shaders_{
      // gbuff
      FragmentShader{ctx_, "shaders/gbuff_frag.spv", "../shaders/gbuff.frag"},

      // ao
      FragmentShader{ctx_, "shaders/ao_frag.spv", "../shaders/ao.frag"},
      FragmentShader{ctx_, "shaders/blur_frag.spv", "../shaders/blur.frag"},

      // light
      FragmentShader{ctx_, "shaders/dir_light_frag.spv",
                     "../shaders/dir_light.frag"},
      FragmentShader{ctx_, "shaders/point_light_frag.spv",
                     "../shaders/point_light.frag"},
      FragmentShader{ctx_, "shaders/spot_light.spv",
                     "../shaders/spot_light.frag"},

      // abuff
      FragmentShader{ctx_, "shaders/abuff_fill_frag.spv",
                     "../shaders/abuff_fill.frag"},
      FragmentShader{ctx_, "shaders/abuff_resolve_frag.spv",
                     "../shaders/abuff_resolve.frag"},

      // wboit
      FragmentShader{ctx_, "shaders/wboit_acc_frag.spv",
                     "../shaders/wboit_acc.frag"},
      FragmentShader{ctx_, "shaders/wboit_resolve_frag.spv",
                     "../shaders/wboit_resolve.frag"},

      // swap
      FragmentShader{ctx_, "shaders/swap_frag.spv", "../shaders/swap.frag"},
  };

  PipelineCache pipe_cache_{ctx_};
  GuiManager gui_manager_{window_, ctx_, swapchain_, swap_pass_, pipe_cache_};

  // pipelines
  GBuffPipe gbuff_pipe_{
      ctx_,
      pipe_cache_,
      gbuff_pass_,
      vert_shaders_[1],
      frag_shaders_[0],
      resrc_.GetProps().samples,
      desc_pool_,
  };

  AOPipe ao_pipe_{
      ctx_,
      pipe_cache_,
      ao_pass_,
      vert_shaders_[0],
      frag_shaders_[1],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  AOBlurPipe ao_blur_pipe_{
      ctx_,
      pipe_cache_,
      ao_blur_v_pass_,
      vert_shaders_[0],
      frag_shaders_[2],
      desc_pool_,
  };

  GlobLightPipe glob_light_pipe_{
      ctx_,
      pipe_cache_,
      glob_light_pass_,
      vert_shaders_[0],
      frag_shaders_[3],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  LocalLightPipe local_light_pipe_{
      ctx_,
      pipe_cache_,
      local_light_pass_,
      vert_shaders_[2],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  PointLightPipe point_light_pipe_{
      ctx_,
      pipe_cache_,
      local_light_pass_,
      vert_shaders_[2],
      frag_shaders_[4],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  SpotLightPipe spot_light_pipe_{
      ctx_,
      pipe_cache_,
      local_light_pass_,
      vert_shaders_[2],
      frag_shaders_[5],
      resrc_.GetProps().samples,
      desc_pool_,
  };

  ABuffFillPipe abuff_fill_pipe_{
      ctx_,
      pipe_cache_,
      abuff_pass_,
      vert_shaders_[1],
      frag_shaders_[6],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  ABuffResolvePipe abuff_res_pipe_{
      ctx_,
      pipe_cache_,
      abuff_pass_,
      vert_shaders_[0],
      frag_shaders_[7],
      resrc_.GetProps().samples,
      desc_pool_,
  };

  WBoitAccPipe wboit_acc_pipe_{
      ctx_,
      pipe_cache_,
      wboit_pass_,
      vert_shaders_[1],
      frag_shaders_[8],
      resrc_.GetProps().samples,
      desc_pool_,
  };
  WBoitResolvePipe wboit_res_pipe_{
      ctx_,       pipe_cache_, wboit_pass_, vert_shaders_[0], frag_shaders_[9],
      desc_pool_,
  };

  SwapPipe swap_pipe_{
      ctx_,       pipe_cache_, swap_pass_, vert_shaders_[0], frag_shaders_[10],
      desc_pool_,
  };
};
}  // namespace npr_graphics

#endif  // RENDERER_H_
