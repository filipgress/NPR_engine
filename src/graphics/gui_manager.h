#ifndef GUI_MANAGER_H_
#define GUI_MANAGER_H_

#include "settings.h"
#include "context.h"
#include "swapchain.h"
#include "render_pass.h"
#include "pipeline_cache.h"
#include "descriptor_pool.h"

#include "core/frame_timer.h"
#include "core/frame_timer.h"
#include "window/window.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace npr_graphics {
struct VulkanHandles {
  vk::Instance instance;
  vk::Device device;
};

class GuiManager : public npr_core::NonCopyable {
 public:
  GuiManager(const npr_window::Window& window, const Context& ctx,
             const Swapchain& swapchain, const SwapPass& swap_pass,
             const PipelineCache& pipeline_cache);
  ~GuiManager();

  void NewFrame(npr_graphics::RenderSettings& settings,
                npr_core::FrameTimer& timer, npr_scene::Camera& camera,
                npr_scene::Scene& scene);

  void RecordUI(vk::CommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  }

  static PFN_vkVoidFunction VulkanLoaderFn(const char* fn_name,
                                           void* user_data);
  static void CheckVkResult(VkResult result);

  void ToggleFileBrowser(std::function<void(std::string)> on_file_selected);

 private:
  void FpsOverlay(float fps);
  void GlobalSettingsWindow(npr_graphics::RenderSettings& settings,
                            npr_core::FrameTimer& timer, float cam_aspect);
  void SceneWindow(npr_scene::Camera& camera, npr_scene::Scene& scene);
  void InspectorWindow(npr_scene::Camera& camera);

  void DrawEntTree(flecs::entity ent, npr_scene::Camera& camera, bool filter,
                   const std::unordered_set<uint64_t>& visible_entities);
  void CalcVisib(npr_scene::Scene& scene, const std::string& search,
                 std::unordered_set<uint64_t>& visible_entities);

  void BrowserWindow();
  void RefreshDirList();
  bool IsGlfwFile(const std::filesystem::path& path);

  void DrawTransformComp(npr_scene::Camera& camera);
  void DrawMeshComp();
  void DrawMaterialComp();
  void DrawBoundingBoxComp();

  void DrawPerspectiveComp(npr_scene::Camera& camera);
  void DrawOrthographicComp(npr_scene::Camera& camera);

  void DrawLightComp();
  void DrawRangeComp();
  void DrawSpotComp();

 private:
  ImGuiDescriptorPool desc_pool_;
  flecs::entity selected_ent_ = flecs::entity::null();

  bool show_browser_{false};
  std::optional<std::filesystem::path> path_;
  std::vector<std::filesystem::path> dir_list_;
  std::string selected_file_;
  std::function<void(std::string)> on_file_selected_;
};

}  // namespace npr_graphics

#endif  // GUI_MANAGER_H_
