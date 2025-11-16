#ifndef GUI_MANAGER_H_
#define GUI_MANAGER_H_

#include "vulkan_context.h"
#include "swapchain.h"
#include "render_pass.h"
#include "pipeline_cache.h"
#include "descriptor_pool.h"

#include "window/window.h"
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
  GuiManager(const npr_window::Window& window, const VulkanContext& context,
             const Swapchain& swapchain, const SwapPass& swap_pass,
             const PipelineCache& pipeline_cache);
  ~GuiManager();

  void NewFrame();
  void Render(vk::CommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  }

  static PFN_vkVoidFunction VulkanLoaderFn(const char* fn_name,
                                           void* user_data);
  static void CheckVkResult(VkResult result);

 private:
  ImGuiDescriptorPool desc_pool_;
};

}  // namespace npr_graphics

#endif  // GUI_MANAGER_H_
