#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

struct VulkanHandles {
  vk::Instance instance;
  vk::Device device;
};

PFN_vkVoidFunction vulkan_loader_func(const char* fn_name, void* user_data) {
  auto* handles = static_cast<VulkanHandles*>(user_data);
  PFN_vkVoidFunction fn = nullptr;

  if (handles->device) {
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr(
        handles->device, fn_name);
    if (fn) return fn;
  }
  if (handles->instance) {
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
        handles->instance, fn_name);
    if (fn) return fn;
  }
  fn = vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
      VK_NULL_HANDLE, fn_name);

  return fn;
}

int main(void) {
  try {
    try {
      vk::detail::defaultDispatchLoaderDynamic.init();
    } catch (const std::exception& e) {
      std::cout << "Loader error: " << e.what() << std::endl;
      exit(-1);
    }

    vk::Instance instance = vk::createInstance({}, nullptr);
    vk::detail::defaultDispatchLoaderDynamic.init(instance);

    std::vector<vk::PhysicalDevice> physicalDevices =
        instance.enumeratePhysicalDevices();
    assert(!physicalDevices.empty());

    vk::Device device = physicalDevices[0].createDevice({}, nullptr);
    vk::detail::defaultDispatchLoaderDynamic.init(device);

    VulkanHandles handles = {instance, device};

    ImGui_ImplVulkan_LoadFunctions(1, vulkan_loader_func, &handles);

  } catch (vk::SystemError const& err) {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit(-1);
  } catch (const std::exception& e) {
    std::cout << "Unknown error: " << e.what() << std::endl;
    exit(-1);
  }

  std::cout << "Done\n";
  return 0;
}
