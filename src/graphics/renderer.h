#ifndef RENDERER_H_
#define RENDERER_H_

#include "vulkan_context.h"
#include "swapchain.h"
#include "command_pool.h"
#include "descriptor_pool.h"
#include "sync.h"
#include "resources.h"
#include "shader.h"
#include "render_pass.h"
#include "pipeline.h"
#include "pipeline_cache.h"

#include "window/window.h"
#include "scene/scene.h"
#include "scene/camera.h"

namespace npr_graphics {
class Renderer : public npr_core::NonCopyable {
 public:
  Renderer(const npr_window::Window& window) : window_{window} {}
  ~Renderer() { Finish(); }

  void Render(const npr_scene::Camera& camera, npr_scene::Scene& scene,
              bool is_loading, float dt);

  const VulkanContext& GetContext() const { return c_; }
  const Resources& GetResources() const { return res_; }

  void Resize() { swapchain_.GetProps().dirty = true; }
  void RecompileShaders();
  void SwapShaders();

  void Finish() { c_.GetDevice().waitIdle(); }

 private:
  vk::CommandBuffer Record(uint image_idx, const npr_scene::Camera& camera,
                           npr_scene::Scene& scene, bool is_loading, float dt);

  void RecordGBufferPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                         const npr_graphics::FrameResources& frame_res,
                         const vk::Extent2D& res_extent,
                         const npr_scene::Camera& camera,
                         npr_scene::Scene& scene);
  void RecordAOPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                    const vk::Extent2D& res_extent);
  void RecordLightPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                       const vk::Extent2D& res_extent);
  void RecordABufferPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                         const npr_graphics::FrameResources& frame_res,
                         const vk::Extent2D& res_extent,
                         const npr_scene::Camera& camera,
                         npr_scene::Scene& scene);
  void RecordWBoitPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                       const npr_graphics::FrameResources& frame_res,
                       const vk::Extent2D& res_extent,
                       const npr_scene::Camera& camera,
                       npr_scene::Scene& scene);
  void RecordSwapPass(vk::CommandBuffer cmd_buff, uint image_idx,
                      uint frame_idx, float camera_aspect, bool is_loading,
                      float dt);

  void RenderTargetResize();
  std::pair<vk::Viewport, vk::Rect2D> CalcViewportScissor(
      vk::Extent2D swap_extent, float camera_aspect) const;

 private:
  const npr_window::Window& window_;

  VulkanContext c_{window_};
  Swapchain swapchain_{c_, window_.GetSize()};

  Sync sync_{c_, swapchain_.GetProps().image_count};
  GraphicsCommandPool cmd_pool_{c_, sync_.GetFrameCount()};

  Resources res_{c_, cmd_pool_, {1280, 720}, sync_.GetFrameCount()};
  DescriptorPool desc_pool_{c_, res_};

  GBuffPass gbuff_pass_{c_, res_};

  AOGenPass ao_pass_{c_, res_};
  AOBlurHPass ao_blur_h_pass_{c_, res_};
  AOBlurVPass ao_blur_v_pass_{c_, res_};

  LightPass light_pass_{c_, res_};
  ABuffPass abuff_pass_{c_, res_};
  WBoitPass wboit_pass_{c_, res_};
  SwapPass swap_pass_{c_, swapchain_};

  std::array<VertexShader, 2> vert_shaders_{
      VertexShader{c_, "shaders/quad_vert.spv", "../shaders/quad.vert"},
      VertexShader{c_, "shaders/gbuff_vert.spv", "../shaders/gbuff.vert"}};

  std::array<FragmentShader, 8> frag_shaders_{
      FragmentShader{c_, "shaders/swap_frag.spv", "../shaders/swap.frag"},
      FragmentShader{c_, "shaders/gbuff_frag.spv", "../shaders/gbuff.frag"},
      FragmentShader{c_, "shaders/abuff_fill_frag.spv",
                     "../shaders/abuff_fill.frag"},
      FragmentShader{c_, "shaders/abuff_resolve_frag.spv",
                     "../shaders/abuff_resolve.frag"},
      FragmentShader{c_, "shaders/wboit_acc_frag.spv",
                     "../shaders/wboit_acc.frag"},
      FragmentShader{c_, "shaders/wboit_compose_frag.spv",
                     "../shaders/wboit_compose.frag"},
      FragmentShader{c_, "shaders/ao_frag.spv", "../shaders/ao.frag"},
      FragmentShader{c_, "shaders/blur_frag.spv", "../shaders/blur.frag"},
  };

  PipelineCache pipe_cache_{c_};
  GBuffPipe gbuff_pipe_{
      c_,
      pipe_cache_,
      gbuff_pass_,
      vert_shaders_[1],
      frag_shaders_[1],
      res_.GetProps().samples,
      desc_pool_,
  };
  AOGenPipe ao_gen_pipe_{
      c_,
      pipe_cache_,
      ao_pass_,
      vert_shaders_[0],
      frag_shaders_[6],
      res_.GetProps().samples,
      desc_pool_,
  };
  AOBlurPipe ao_blur_pipe_{
      c_,
      pipe_cache_,
      ao_blur_v_pass_,
      vert_shaders_[0],
      frag_shaders_[7],
      desc_pool_,
  };
  ABuffFillPipe abuff_fill_pipe_{
      c_,
      pipe_cache_,
      abuff_pass_,
      vert_shaders_[1],
      frag_shaders_[2],
      res_.GetProps().samples,
      desc_pool_,
  };
  ABuffResolvePipe abuff_resolve_pipe_{
      c_,
      pipe_cache_,
      abuff_pass_,
      vert_shaders_[0],
      frag_shaders_[3],
      res_.GetProps().samples,
      desc_pool_,
  };
  WBoitAccPipe wboit_acc_pipe_{
      c_,
      pipe_cache_,
      wboit_pass_,
      vert_shaders_[1],
      frag_shaders_[4],
      res_.GetProps().samples,
      desc_pool_,
  };
  WBoitComposePipe wboit_compose_pipe_{
      c_,         pipe_cache_, wboit_pass_, vert_shaders_[0], frag_shaders_[5],
      desc_pool_,
  };
  SwapPipe swap_pipe_{
      c_,         pipe_cache_, swap_pass_, vert_shaders_[0], frag_shaders_[0],
      desc_pool_,
  };
};
}  // namespace npr_graphics

#endif  // RENDERER_H_
