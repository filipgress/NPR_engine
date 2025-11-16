#ifndef VULKAN_CONTEXT_H_
#define VULKAN_CONTEXT_H_

#include "window/window.h"

namespace npr_graphics {

struct SwapSupport {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> present_modes;

  bool IsAdequate() { return !formats.empty() && !present_modes.empty(); }
};

struct QFamilies {
  std::optional<uint32_t> graphics_i;
  std::optional<uint32_t> present_i;
  std::optional<uint32_t> transfer_i;

  bool IsComplete() {
    return graphics_i.has_value() && present_i.has_value() &&
           transfer_i.has_value();
  }
};

class VulkanContext : public npr_core::NonCopyable {
 public:
  VulkanContext(const npr_window::Window& window);
  ~VulkanContext();

  uint32_t GetAPIVersion() const { return api_version_; }
  vk::Instance GetInstance() const { return instance_; }
  vk::PhysicalDevice GetPhysicalDevice() const { return phys_device_; }
  vk::Device GetDevice() const { return device_; }
  vk::SurfaceKHR GetSurface() const { return surface_; }

  vk::Queue GetGraphicsQ() const { return graphics_q_; }
  vk::Queue GetPresentQ() const { return present_q_; }
  vk::Queue GetTransferQ() const { return transfer_q_; }

  SwapSupport GetSwapSupp() const { return GetSwapSupport(phys_device_); }
  QFamilies GetQFamilies() const { return q_families_; }

  vk::PhysicalDeviceProperties GetProperties() const {
    return phys_device_.getProperties();
  }
  vk::FormatProperties GetFormatProperties(vk::Format format) const {
    return phys_device_.getFormatProperties(format);
  }

  uint32_t FindMemTypeIdx(uint32_t typ_bits,
                          vk::MemoryPropertyFlags properties) const;
  vk::Format FindFormat(const std::vector<vk::Format>& candidates,
                        vk::ImageTiling tiling,
                        vk::FormatFeatureFlags features) const;

  void SetDbgName(uint64_t object_handle, vk::ObjectType object_type,
                  const std::string& name) const;

 private:
  void CreateInstance();

  bool ExtsSupported() const;
  bool LayersSupported() const;

  void CreateDbgMessenger();
  vk::DebugUtilsMessengerCreateInfoEXT GetDbgMessengerInfo() const;

  static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanErrorCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
      vk::DebugUtilsMessageTypeFlagsEXT message_type,
      const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void* pUserData);

  void PickPhysDevice();
  void CreateDevice();

  int RateDevice(vk::PhysicalDevice device) const;

  bool DeviceExtsSupported(vk::PhysicalDevice device) const;
  bool DeviceFeatsSupported(vk::PhysicalDevice device) const;

  SwapSupport GetSwapSupport(vk::PhysicalDevice device) const;
  QFamilies GetQueueFamilies(vk::PhysicalDevice device) const;

 private:
  uint32_t api_version_{0};

  vk::Instance instance_{nullptr};
  vk::SurfaceKHR surface_{nullptr};

  vk::DebugUtilsMessengerEXT dbg_messenger_{nullptr};

  vk::PhysicalDevice phys_device_{nullptr};
  vk::Device device_{nullptr};

  QFamilies q_families_;
  vk::Queue graphics_q_{nullptr};
  vk::Queue present_q_{nullptr};
  vk::Queue transfer_q_{nullptr};

  std::vector<const char*> layers_;
  std::vector<const char*> exts_{VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME};

  vk::PhysicalDeviceFeatures device_feats_{};
  std::vector<const char*> device_exts_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};
}  // namespace npr_graphics

#endif  // VULKAN_CONTEXT_H_
