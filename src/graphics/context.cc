#include "context.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace npr_graphics {
Context::Context(const npr_window::Window& window) {
  vk::detail::defaultDispatchLoaderDynamic.init();

  CreateInstance();
  CreateDbgMessenger();

  surface_ = window.CreateSurface(instance_);

  PickPhysDevice();
  CreateDevice();
}

Context::~Context() {
  if (device_) device_.destroy();

  if (instance_) {
    if (surface_) instance_.destroySurfaceKHR(surface_);
    if (kEnableDebug && dbg_messenger_)
      instance_.destroyDebugUtilsMessengerEXT(dbg_messenger_);
    instance_.destroy();
  }
}

void Context::CreateInstance() {
  api_ver_ = vk::enumerateInstanceVersion();
  api_ver_ &= ~0xFFFU;  // zero out patch number

  INFO(PROJECT_NAME, " version: ", PROJECT_VERSION_MAJOR, ".",
       PROJECT_VERSION_MINOR);
  INFO("vulkan version: ", VK_API_VERSION_MAJOR(api_ver_), ".",
       VK_API_VERSION_MINOR(api_ver_));

  vk::ApplicationInfo app_info{};
  app_info.pApplicationName = PROJECT_NAME;
  app_info.applicationVersion =
      VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, 0);
  app_info.apiVersion = api_ver_;

  uint32_t exts_count{0};
  const char** req_exts = glfwGetRequiredInstanceExtensions(&exts_count);
  exts_.insert(exts_.end(), req_exts, req_exts + exts_count);

  if (kEnableDebug) {
    exts_.push_back("VK_EXT_debug_utils");
    layers_.push_back("VK_LAYER_KHRONOS_validation");
  }

  vk::InstanceCreateInfo inst_info{};
  inst_info.pApplicationInfo = &app_info;
  inst_info.enabledExtensionCount = exts_.size();
  inst_info.ppEnabledExtensionNames = exts_.data();
  inst_info.enabledLayerCount = layers_.size();
  inst_info.ppEnabledLayerNames = layers_.data();

  if (kEnableDebug) {
    auto debug_info = GetDbgMessengerInfo();
    inst_info.pNext = &debug_info;
  }

  if (!ExtsSupported() || !LayersSupported())
    throw std::runtime_error("Unable to create instance");

  instance_ = vk::createInstance(inst_info);
  vk::detail::defaultDispatchLoaderDynamic.init(instance_);
}

void Context::CreateDevice() {
  q_families_ = GetQFamilies(phys_device_);

  std::set<uint32_t> unique_q_indices{
      q_families_.graphics_i.value(),
      q_families_.present_i.value(),
      q_families_.transfer_i.value(),
  };

  std::vector<vk::DeviceQueueCreateInfo> queue_infos;
  float queue_priority{1.0f};
  for (uint32_t queue_index : unique_q_indices) {
    vk::DeviceQueueCreateInfo queue_info;
    queue_info.flags = vk::DeviceQueueCreateFlags();
    queue_info.pQueuePriorities = &queue_priority;
    queue_info.queueCount = 1;
    queue_info.queueFamilyIndex = queue_index;

    queue_infos.push_back(queue_info);
  }

  vk::DeviceCreateInfo device_info{};
  device_info.queueCreateInfoCount = queue_infos.size();
  device_info.pQueueCreateInfos = queue_infos.data();
  device_info.enabledExtensionCount = device_exts_.size();
  device_info.ppEnabledExtensionNames = device_exts_.data();
  device_info.pEnabledFeatures = &device_feats_;

  device_ = phys_device_.createDevice(device_info);
  vk::detail::defaultDispatchLoaderDynamic.init(device_);

  graphics_q_ = device_.getQueue(q_families_.graphics_i.value(), 0);
  present_q_ = device_.getQueue(q_families_.present_i.value(), 0);
  transfer_q_ = device_.getQueue(q_families_.transfer_i.value(), 0);

  INFO("queues (graphics, present, transfer): ", q_families_.graphics_i.value(),
       ", ", q_families_.present_i.value(), ", ",
       q_families_.transfer_i.value());
}

/*
 * Set up Vulkan Debug Messenger and Error callback function
 */
void Context::CreateDbgMessenger() {
  if (!kEnableDebug) return;

  auto debug_info = GetDbgMessengerInfo();
  dbg_messenger_ = instance_.createDebugUtilsMessengerEXT(debug_info);
}

vk::DebugUtilsMessengerCreateInfoEXT Context::GetDbgMessengerInfo() const {
  vk::DebugUtilsMessengerCreateInfoEXT debug_info{};
  debug_info.flags = vk::DebugUtilsMessengerCreateFlagsEXT();
  debug_info.messageSeverity =
      // vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
      // vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

  debug_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
  debug_info.pfnUserCallback = VulkanErrorCallback;

  return debug_info;
}

VKAPI_ATTR uint32_t VKAPI_CALL Context::VulkanErrorCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
  (void)pUserData, (void)messageSeverity;
  ERR("[VK_VALIDATION_LAYER] ", static_cast<uint32_t>(messageType), ": ",
      pCallbackData->pMessage);
  return VK_FALSE;
}

/*
 * Select best physical device
 */
void Context::PickPhysDevice() {
  // required device features
  device_feats_.independentBlend = VK_TRUE;          // WBOIT
  device_feats_.sampleRateShading = VK_TRUE;         // MSAA
  device_feats_.samplerAnisotropy = VK_TRUE;         // Anisotropic filtering
  device_feats_.fragmentStoresAndAtomics = VK_TRUE;  // K-buffer

  std::vector<vk::PhysicalDevice> devices =
      instance_.enumeratePhysicalDevices();
  std::multimap<int, vk::PhysicalDevice> candidates;

  if (devices.size() == 0)
    throw std::runtime_error("Failed to find GPU with Vulkan support");

  for (const auto& device : devices)
    candidates.insert(std::pair(RateDevice(device), device));

  if (candidates.rbegin()->first == 0)
    throw std::runtime_error("Failed to find suitable GPU");

  phys_device_ = candidates.rbegin()->second;
}

int Context::RateDevice(vk::PhysicalDevice device) const {
  int score{1};

  if (!DeviceExtsSupported(device) || !DeviceFeatsSupported(device)) return 0;

  auto swap_supp = GetSwapSupp(device);
  auto q_families = GetQFamilies(device);

  if (!swap_supp.IsValid() || !q_families.IsComplete()) return 0;

  auto props = device.getProperties();
  if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 100;
  if (q_families.transfer_i != q_families.graphics_i) score += 10;

  return score;
}

/*
 * Check for extension/layer/features support
 */
bool Context::LayersSupported() const {
  std::unordered_set<std::string> required_layers{layers_.begin(),
                                                  layers_.end()};

  for (const auto& layer : vk::enumerateInstanceLayerProperties())
    required_layers.erase(layer.layerName);

  if (required_layers.empty()) return true;

  for (const std::string& unsupported : required_layers)
    ERR("Layer: ", unsupported, "is not supported");
  return false;
}

bool Context::ExtsSupported() const {
  std::unordered_set<std::string> required_exts{exts_.begin(), exts_.end()};

  for (const auto& extension : vk::enumerateInstanceExtensionProperties())
    required_exts.erase(extension.extensionName);

  if (required_exts.empty()) return true;

  for (const std::string& unsupported : required_exts)
    ERR("Extension: ", unsupported, " is not supported");
  return false;
}

bool Context::DeviceExtsSupported(vk::PhysicalDevice device) const {
  std::unordered_set<std::string> required_exts{device_exts_.begin(),
                                                device_exts_.end()};
  for (const auto& extension : device.enumerateDeviceExtensionProperties())
    required_exts.erase(extension.extensionName);

  return required_exts.empty();
}

#define CHECK_DEVICE_FEATURE(FEATURE) \
  if (device_feats_.FEATURE && !supported.FEATURE) return false;

bool Context::DeviceFeatsSupported(vk::PhysicalDevice device) const {
  vk::PhysicalDeviceFeatures supported = device.getFeatures();

  CHECK_DEVICE_FEATURE(robustBufferAccess)
  CHECK_DEVICE_FEATURE(fullDrawIndexUint32)
  CHECK_DEVICE_FEATURE(imageCubeArray)
  CHECK_DEVICE_FEATURE(independentBlend)
  CHECK_DEVICE_FEATURE(geometryShader)
  CHECK_DEVICE_FEATURE(tessellationShader)
  CHECK_DEVICE_FEATURE(sampleRateShading)
  CHECK_DEVICE_FEATURE(dualSrcBlend)
  CHECK_DEVICE_FEATURE(logicOp)
  CHECK_DEVICE_FEATURE(multiDrawIndirect)
  CHECK_DEVICE_FEATURE(drawIndirectFirstInstance)
  CHECK_DEVICE_FEATURE(depthClamp)
  CHECK_DEVICE_FEATURE(depthBiasClamp)
  CHECK_DEVICE_FEATURE(fillModeNonSolid)
  CHECK_DEVICE_FEATURE(depthBounds)
  CHECK_DEVICE_FEATURE(wideLines)
  CHECK_DEVICE_FEATURE(largePoints)
  CHECK_DEVICE_FEATURE(alphaToOne)
  CHECK_DEVICE_FEATURE(multiViewport)
  CHECK_DEVICE_FEATURE(samplerAnisotropy)
  CHECK_DEVICE_FEATURE(textureCompressionETC2)
  CHECK_DEVICE_FEATURE(textureCompressionASTC_LDR)
  CHECK_DEVICE_FEATURE(textureCompressionBC)
  CHECK_DEVICE_FEATURE(occlusionQueryPrecise)
  CHECK_DEVICE_FEATURE(pipelineStatisticsQuery)
  CHECK_DEVICE_FEATURE(vertexPipelineStoresAndAtomics)
  CHECK_DEVICE_FEATURE(fragmentStoresAndAtomics)
  CHECK_DEVICE_FEATURE(shaderTessellationAndGeometryPointSize)
  CHECK_DEVICE_FEATURE(shaderImageGatherExtended)
  CHECK_DEVICE_FEATURE(shaderStorageImageExtendedFormats)
  CHECK_DEVICE_FEATURE(shaderStorageImageMultisample)
  CHECK_DEVICE_FEATURE(shaderStorageImageReadWithoutFormat)
  CHECK_DEVICE_FEATURE(shaderStorageImageWriteWithoutFormat)
  CHECK_DEVICE_FEATURE(shaderUniformBufferArrayDynamicIndexing)
  CHECK_DEVICE_FEATURE(shaderSampledImageArrayDynamicIndexing)
  CHECK_DEVICE_FEATURE(shaderStorageBufferArrayDynamicIndexing)
  CHECK_DEVICE_FEATURE(shaderStorageImageArrayDynamicIndexing)
  CHECK_DEVICE_FEATURE(shaderClipDistance)
  CHECK_DEVICE_FEATURE(shaderCullDistance)
  CHECK_DEVICE_FEATURE(shaderFloat64)
  CHECK_DEVICE_FEATURE(shaderInt64)
  CHECK_DEVICE_FEATURE(shaderInt16)
  CHECK_DEVICE_FEATURE(shaderResourceResidency)
  CHECK_DEVICE_FEATURE(shaderResourceMinLod)
  CHECK_DEVICE_FEATURE(sparseBinding)
  CHECK_DEVICE_FEATURE(sparseResidencyBuffer)
  CHECK_DEVICE_FEATURE(sparseResidencyImage2D)
  CHECK_DEVICE_FEATURE(sparseResidencyImage3D)
  CHECK_DEVICE_FEATURE(sparseResidency2Samples)
  CHECK_DEVICE_FEATURE(sparseResidency4Samples)
  CHECK_DEVICE_FEATURE(sparseResidency8Samples)
  CHECK_DEVICE_FEATURE(sparseResidency16Samples)
  CHECK_DEVICE_FEATURE(sparseResidencyAliased)
  CHECK_DEVICE_FEATURE(variableMultisampleRate)
  CHECK_DEVICE_FEATURE(inheritedQueries)

  return true;
}

/*
 * Query physical device attributes
 */
SwapSupp Context::GetSwapSupp(vk::PhysicalDevice device) const {
  SwapSupp swap_supp;
  swap_supp.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
  swap_supp.formats = device.getSurfaceFormatsKHR(surface_);
  swap_supp.present_modes = device.getSurfacePresentModesKHR(surface_);

  return swap_supp;
}

QFamilies Context::GetQFamilies(vk::PhysicalDevice device) const {
  QFamilies q_families;

  uint32_t i{0};
  for (const auto& queueFamily : device.getQueueFamilyProperties()) {
    if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
      q_families.graphics_i = i;
    if (device.getSurfaceSupportKHR(i, surface_)) q_families.present_i = i;
    if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer &&
        !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
      q_families.transfer_i = i;

    if (q_families.IsComplete()) break;
    i++;
  }

  if (!q_families.transfer_i.has_value())
    q_families.transfer_i = q_families.graphics_i;

  return q_families;
}

uint32_t Context::FindMemTypeIdx(uint32_t type_bits,
                                 vk::MemoryPropertyFlags properties) const {
  auto mem_props = phys_device_.getMemoryProperties();

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++)
    if ((type_bits & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
      return i;

  throw std::runtime_error("Failed to find suitable memory type!");
}

vk::Format Context::FindFormat(const std::vector<vk::Format>& candidates,
                               vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features) const {
  for (vk::Format format : candidates) {
    vk::FormatProperties props = phys_device_.getFormatProperties(format);

    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == vk::ImageTiling::eOptimal &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

void Context::SetDbgName(uint64_t object_handle, vk::ObjectType object_type,
                         const std::string& name) const {
  if (kEnableDebug) {
    vk::DebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.objectType = object_type;
    nameInfo.objectHandle = object_handle;
    nameInfo.pObjectName = name.c_str();

    try {
      device_.setDebugUtilsObjectNameEXT(nameInfo);
    } catch (const vk::SystemError& err) {
      INFO("Failed to set debug name: ", err.what());
    }
  }
}
}  // namespace npr_graphics
