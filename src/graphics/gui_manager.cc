#include "gui_manager.h"

namespace npr_graphics {

PFN_vkVoidFunction GuiManager::VulkanLoaderFn(const char* fn_name,
                                              void* user_data) {
  auto* handles = static_cast<VulkanHandles*>(user_data);
  PFN_vkVoidFunction fn = nullptr;

  if (handles->instance) {
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
        handles->instance, fn_name);
    if (fn) return fn;
  }
  if (handles->device) {
    INFO("HERE");
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr(
        handles->device, fn_name);
    if (fn) return fn;
  }
  return vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
      VK_NULL_HANDLE, fn_name);
}

void GuiManager::CheckVkResult(VkResult result) {
  if (result != VK_SUCCESS)
    throw std::runtime_error("Vulkan Error: VkResult = " +
                             std::to_string(result));
}

GuiManager::GuiManager(const npr_window::Window& window,
                       const VulkanContext& context, const Swapchain& swapchain,
                       const SwapPass& swap_pass,
                       const PipelineCache& pipeline_cache)
    : desc_pool_{context} {
  // init imgui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  VulkanHandles handles{context.GetInstance(), context.GetDevice()};
  ImGui_ImplVulkan_LoadFunctions(context.GetAPIVersion(),
                                 GuiManager::VulkanLoaderFn, &handles);

  // init glfw backend
  ImGui_ImplGlfw_InitForVulkan(window.GetNative(), true);

  // init vulkan backend
  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = context.GetAPIVersion();
  init_info.Instance = context.GetInstance();
  init_info.PhysicalDevice = context.GetPhysicalDevice();
  init_info.Device = context.GetDevice();
  init_info.QueueFamily = context.GetQFamilies().graphics_i.value();
  init_info.Queue = context.GetGraphicsQ();
  init_info.PipelineCache = pipeline_cache.GetCache();
  init_info.DescriptorPool = desc_pool_.GetPool();
  init_info.RenderPass = swap_pass.GetRenderPass();
  init_info.Subpass = 1;
  init_info.MinImageCount = swapchain.GetProps().min_image_count;
  init_info.ImageCount = swapchain.GetProps().image_count;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = CheckVkResult;

  ImGui_ImplVulkan_Init(&init_info);
}

GuiManager::~GuiManager() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void GuiManager::NewFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::ShowDemoWindow();
  ImGui::Begin("Color Test");
  ImVec4 red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);    // Full red
  ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Full green
  ImVec4 blue = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);   // Full blue
  ImVec4 gray = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);   // Mid-gray
  ImGui::ColorButton("Red", red);
  ImGui::ColorButton("Green", green);
  ImGui::ColorButton("Blue", blue);
  ImGui::ColorButton("Gray", gray);
  ImGui::End();
}

}  // namespace npr_graphics
