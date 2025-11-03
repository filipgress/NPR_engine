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
  Resources& GetResources() { return res_; }

  void Resize() { swapchain_.GetProps().dirty = true; }
  void RecompileShaders();
  void SwapShaders();

  void Finish() { c_.GetDevice().waitIdle(); }

 private:
  vk::CommandBuffer Record(uint image_idx);
  vk::CommandBuffer RecordFallback(uint image_idx, float camera_aspect,
                                   bool is_loading, float dt);

  void RenderTargetResize();
  std::pair<vk::Viewport, vk::Rect2D> CalcViewportScissor(
      vk::Extent2D swap_extent, float camera_aspect) const;

 private:
  const npr_window::Window& window_;

  VulkanContext c_{window_};
  Swapchain swapchain_{c_, window_.GetSize()};

  Sync sync_{c_, swapchain_.GetProps().image_count};
  Resources res_{c_, {1280, 720}, sync_.GetFrameCount()};

  GraphicsCommandPool cmd_pool_{c_, sync_.GetFrameCount()};
  DescriptorPool desc_pool_{c_, res_};

  GBuffPass gbuff_pass_{c_, res_};
  LoadPass load_pass_{c_, res_};
  SwapPass swap_pass_{c_, swapchain_};

  std::array<VertexShader, 2> vert_shaders_{
      VertexShader{c_, "shaders/gbuff_vert.spv", "../shaders/gbuff.vert"},
      VertexShader{c_, "shaders/quad_vert.spv", "../shaders/quad.vert"}};

  std::array<FragmentShader, 3> frag_shaders_{
      FragmentShader{c_, "shaders/gbuff_frag.spv", "../shaders/gbuff.frag"},
      FragmentShader{c_, "shaders/swap_frag.spv", "../shaders/swap.frag"},
      FragmentShader{c_, "shaders/main_frag.spv", "../shaders/main.frag"},
  };

  PipelineCache pipe_cache_{c_};
  GBuffPipe gbuff_pipe_{c_,
                        pipe_cache_,
                        gbuff_pass_,
                        vert_shaders_[0],
                        frag_shaders_[0],
                        res_.GetProps(),
                        desc_pool_};
  LoadPipe load_pipe_{c_, pipe_cache_, load_pass_, vert_shaders_[1],
                      frag_shaders_[2]};
  SwapPipe swap_pipe_{
      c_,        pipe_cache_, swap_pass_, vert_shaders_[1], frag_shaders_[1],
      desc_pool_};
};
}  // namespace npr_graphics

#endif  // RENDERER_H_
