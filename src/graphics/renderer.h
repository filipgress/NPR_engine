#ifndef RENDERER_H_
#define RENDERER_H_

#include "vulkan_context.h"
#include "swapchain.h"
#include "command_pool.h"
#include "sync.h"
#include "resources.h"
#include "shader.h"
#include "render_pass.h"
#include "pipeline.h"
#include "pipeline_cache.h"

namespace npr_graphics {
class Renderer : public npr_core::NonCopyable {
 public:
  Renderer(const npr_window::Window& window) : window_{window} {}
  ~Renderer() { Finish(); }

  void Render();

  const VulkanContext& GetContext() const { return c_; }

  void OnWindowResize() { swapchain_.GetProps().dirty = true; }
  void RecompileShaders();
  void SwapShaders();

  void Finish() { c_.GetDevice().waitIdle(); }

 private:
  vk::CommandBuffer Record(uint image_idx);
  void RenderTargetResize();

 private:
  const npr_window::Window& window_;

  VulkanContext c_{window_};
  Swapchain swapchain_{c_, window_.GetSize()};

  Sync sync_{c_, swapchain_.GetProps().image_count};
  Resources res_{c_, {1280, 720}, sync_.GetFrameCount()};

  GraphicsCommandPool cmd_pool_{c_, sync_.GetFrameCount()};
  DescriptorPool desc_pool_{c_, res_};

  SwapPass swap_pass_{c_, swapchain_};

  std::array<VertexShader, 1> vert_shaders_{
      {{c_, "shaders/main_vert.spv", "../shaders/main.vert"}}};
  std::array<FragmentShader, 1> frag_shaders_{
      {{c_, "shaders/main_frag.spv", "../shaders/main.frag"}}};

  PipelineCache pipe_cache_{c_};
  SwapPipe swap_pipe_{c_, pipe_cache_, swap_pass_, vert_shaders_[0],
                      frag_shaders_[0]};
};
}  // namespace npr_graphics

#endif  // RENDERER_H_
