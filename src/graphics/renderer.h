#ifndef RENDERER_H_
#define RENDERER_H_

#include "settings.h"
#include "context.h"
#include "swapchain.h"
#include "sync.h"
#include "command_pool.h"
#include "resources.h"
#include "descriptor_pool.h"

#include "pass_manager.h"
#include "shader_manager.h"
#include "pipe_manager.h"

#include "gui_manager.h"

#include "core/frame_timer.h"
#include "window/window.h"
#include "scene/scene.h"
#include "scene/camera.h"

namespace npr_graphics {
class Renderer : public npr_core::NonCopyable {
 public:
  Renderer(const npr_window::Window& window) : window_{window} {}
  ~Renderer() { WaitIdle(); }

  void Render(npr_core::FrameTimer& timer, npr_scene::Camera& camera,
              npr_scene::Scene& scene, bool is_loading);

  const Context& GetContext() const { return ctx_; }
  const Resources& GetResrc() const { return resrc_; }
  GuiManager& GetGui() { return gui_; }

  void Update();

  void WaitIdle() { ctx_.GetDevice().waitIdle(); }
  void Resize() { swapchain_.GetProps().dirty = true; }

  void RecompileShaders() { shaders_.Recompile(); };

 private:
  void SwapTargetResize();

  std::pair<vk::Viewport, vk::Rect2D> CalcViewportScissor(
      vk::Extent2D swap_extent, const npr_scene::Camera& camera) const;

  vk::CommandBuffer Record(uint image_idx, const npr_scene::Camera& camera,
                           npr_scene::Scene& scene, bool is_loading, float dt);

  void RecordGBuff(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const npr_graphics::FrameResources& frame_resrc,
                   const vk::Extent2D& resrc_extent,
                   const npr_scene::Camera& camera, npr_scene::Scene& scene);
  void RecordOpaque(vk::CommandBuffer cmd_buff, const uint frame_idx,
                    const npr_graphics::FrameResources& frame_resrc,
                    const npr_scene::Scene& scene,
                    const npr_scene::Frustum& frustum) const;

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

  void RecordBloom(vk::CommandBuffer cmd_buff, const uint frame_idx,
                   const vk::Extent2D& resrc_extent);
  void RecordDoF(vk::CommandBuffer cmd_buff, const uint frame_idx,
                 const vk::Extent2D& resrc_extent,
                 const npr_scene::Camera& camera);

  void RecordSwap(vk::CommandBuffer cmd_buff, uint image_idx,
                  const uint frame_idx, const npr_scene::Camera& camera,
                  bool is_loading, float dt);

 private:
  static uint mat_at;   // cycling through material descriptor sets
  static uint inst_at;  // cycling through instance descriptor sets

  RenderSettings settings_;

  const npr_window::Window& window_;

  Context ctx_{window_};
  Swapchain swapchain_{ctx_, window_.GetSize()};

  Sync sync_{ctx_, swapchain_.GetProps().image_count};
  GraphicsCommandPool cmd_pool_{ctx_, sync_.GetFrameCount(), "renderer"};

  Resources resrc_{ctx_, cmd_pool_, settings_, sync_.GetFrameCount()};
  DescriptorPool desc_pool_{ctx_, resrc_};

  PassManager passes_{ctx_, swapchain_, resrc_};
  ShaderManager shaders_{ctx_};
  PipeManager pipelines_{ctx_, resrc_, desc_pool_, passes_, shaders_};

  GuiManager gui_{window_, ctx_, swapchain_, passes_.swap_,
                  pipelines_.pipe_cache_};
};
}  // namespace npr_graphics

#endif  // RENDERER_H_
