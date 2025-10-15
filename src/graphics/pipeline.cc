#include "pipeline.h"
#include <cstring>

namespace npr_graphics {

// Magic number for pipeline cache identification
constexpr uint32_t kPipelineCacheMagic = 0x504C434E;  // "PLCN"

uint64_t Pipeline::ComputeHash(const void* data, size_t size) {
  // Simple FNV-1a hash for data integrity checking
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  uint64_t hash = 0xcbf29ce484222325ULL;
  
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 0x100000001b3ULL;
  }
  
  return hash;
}

bool Pipeline::LoadPipelineCache() {
  std::ifstream file(cache_file_path_, std::ios::binary);
  if (!file.is_open()) {
    INFO("Pipeline cache file not found, will create new cache");
    return false;
  }

  // Read and validate header
  PipelineCachePrefixHeader header;
  file.read(reinterpret_cast<char*>(&header), sizeof(header));
  
  if (!file || file.gcount() != sizeof(header)) {
    ERR("Failed to read pipeline cache header");
    return false;
  }

  // Validate magic number
  if (header.magic_number != kPipelineCacheMagic) {
    ERR("Invalid pipeline cache magic number");
    return false;
  }

  // Validate device properties
  if (header.vendor_id != device_properties_.vendorID ||
      header.device_id != device_properties_.deviceID ||
      header.driver_version != device_properties_.driverVersion) {
    INFO("Pipeline cache is for different device/driver, creating new cache");
    return false;
  }

  // Validate UUID
  if (std::memcmp(header.uuid, device_properties_.pipelineCacheUUID, 
                  VK_UUID_SIZE) != 0) {
    INFO("Pipeline cache UUID mismatch, creating new cache");
    return false;
  }

  // Read cache data
  std::vector<uint8_t> cache_data(header.data_size);
  file.read(reinterpret_cast<char*>(cache_data.data()), header.data_size);
  
  if (!file || static_cast<size_t>(file.gcount()) != header.data_size) {
    ERR("Failed to read pipeline cache data");
    return false;
  }

  // Validate hash
  uint64_t computed_hash = ComputeHash(cache_data.data(), cache_data.size());
  if (computed_hash != header.hash) {
    ERR("Pipeline cache data corrupted (hash mismatch)");
    return false;
  }

  // Create pipeline cache with loaded data
  vk::PipelineCacheCreateInfo cache_info{};
  cache_info.initialDataSize = cache_data.size();
  cache_info.pInitialData = cache_data.data();

  try {
    pipeline_cache_ = c_.GetDevice().createPipelineCache(cache_info);
    INFO("Successfully loaded pipeline cache from file");
    return true;
  } catch (const vk::SystemError& e) {
    ERR("Failed to create pipeline cache with loaded data: ", e.what());
    return false;
  }
}

void Pipeline::SavePipelineCache() {
  if (!pipeline_cache_) {
    return;
  }

  try {
    // Get cache data from Vulkan
    std::vector<uint8_t> cache_data = c_.GetDevice().getPipelineCacheData(pipeline_cache_);
    
    if (cache_data.empty()) {
      INFO("Pipeline cache is empty, skipping save");
      return;
    }

    // Compute hash
    uint64_t hash = ComputeHash(cache_data.data(), cache_data.size());

    // Prepare header
    PipelineCachePrefixHeader header{};
    header.magic_number = kPipelineCacheMagic;
    header.data_size = static_cast<uint32_t>(cache_data.size());
    header.hash = hash;
    header.vendor_id = device_properties_.vendorID;
    header.device_id = device_properties_.deviceID;
    header.driver_version = device_properties_.driverVersion;
    header.driver_abi = device_properties_.driverVersion;  // Using driver version as ABI
    std::memcpy(header.uuid, device_properties_.pipelineCacheUUID, VK_UUID_SIZE);

    // Write to temporary file first (atomic write)
    std::string temp_path = cache_file_path_ + ".tmp";
    std::ofstream temp_file(temp_path, std::ios::binary);
    
    if (!temp_file.is_open()) {
      ERR("Failed to open temporary cache file for writing");
      return;
    }

    temp_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    temp_file.write(reinterpret_cast<const char*>(cache_data.data()), 
                    cache_data.size());
    temp_file.close();

    if (!temp_file) {
      ERR("Failed to write pipeline cache to temporary file");
      std::filesystem::remove(temp_path);
      return;
    }

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(temp_path, cache_file_path_, ec);
    
    if (ec) {
      ERR("Failed to rename temporary cache file: ", ec.message());
      std::filesystem::remove(temp_path);
      return;
    }

    INFO("Successfully saved pipeline cache to file");
  } catch (const vk::SystemError& e) {
    ERR("Failed to get pipeline cache data: ", e.what());
  } catch (const std::exception& e) {
    ERR("Error saving pipeline cache: ", e.what());
  }
}

void Pipeline::Destroy() {
  // Save pipeline cache before destroying
  if (pipeline_cache_) {
    SavePipelineCache();
    c_.GetDevice().destroyPipelineCache(pipeline_cache_);
    pipeline_cache_ = nullptr;
  }
  
  if (pipeline_) {
    c_.GetDevice().destroyPipeline(pipeline_);
    pipeline_ = nullptr;
  }
  if (layout_) {
    c_.GetDevice().destroyPipelineLayout(layout_);
    layout_ = nullptr;
  }
}

void Pipeline::CreateLayout(
    const std::vector<vk::DescriptorSetLayout>& set_layouts,
    const std::vector<PushConstInfo>& push_consts) {
  vk::PipelineLayoutCreateInfo pipeline_layout_info{};

  pipeline_layout_info.setLayoutCount = set_layouts.size();
  pipeline_layout_info.pSetLayouts = set_layouts.data();

  std::vector<vk::PushConstantRange> push_constant_ranges{};
  push_constant_ranges.reserve(push_consts.size());

  size_t offset{0};
  for (const auto& pc : push_consts) {
    if (pc.size == 0) continue;

    vk::PushConstantRange range{};
    range.stageFlags = pc.stage;
    range.offset = offset;
    range.size = pc.size;
    push_constant_ranges.push_back(range);

    offset += (pc.size + 3) & ~3;  // Align to 4 bytes
  }

  pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
  pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

  layout_ = c_.GetDevice().createPipelineLayout(pipeline_layout_info);
  c_.SetDbgName((uint64_t)(VkPipelineLayout)layout_,
                vk::ObjectType::ePipelineLayout,
                std::string(GetDbgName()) + "_Layout");
}

void Pipeline::CreatePipeline(size_t subpass, bool use_vbo,
                              const std::string& vert_entry,
                              const std::string& frag_entry,
                              const std::vector<SpecConstInfo>& spec_consts) {
  // Create or load pipeline cache
  if (!pipeline_cache_) {
    if (!LoadPipelineCache()) {
      // If loading failed, create empty cache
      vk::PipelineCacheCreateInfo cache_info{};
      try {
        pipeline_cache_ = c_.GetDevice().createPipelineCache(cache_info);
        INFO("Created new empty pipeline cache");
      } catch (const vk::SystemError& e) {
        ERR("Failed to create pipeline cache: ", e.what());
        // Continue without cache
      }
    }
  }

  PipelineData data;

  auto shader_stages =
      GetShaderStages(data, vert_entry, frag_entry, spec_consts);
  auto vertex_input = GetVertexInputState(data, use_vbo);
  auto input_assembly = GetInputAssemblyState();
  auto viewport = GetViewportState();
  auto rasterization = GetRasterizationState();
  auto multisample = GetMultisampleState();
  auto depth_stencil = GetDepthStencilState();
  auto color_blend = GetColorBlendState(data);
  auto dynamic_state = GetDynamicState(data);

  vk::GraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.stageCount = shader_stages.size();
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport;
  pipeline_info.pRasterizationState = &rasterization;
  pipeline_info.pMultisampleState = &multisample;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blend;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = layout_;
  pipeline_info.renderPass = render_pass_.GetRenderPass();
  pipeline_info.subpass = subpass;

  // Use pipeline cache if available
  vk::PipelineCache cache_handle = pipeline_cache_ ? pipeline_cache_ : VK_NULL_HANDLE;
  
  try {
    pipeline_ = c_.GetDevice()
                    .createGraphicsPipeline(cache_handle, pipeline_info)
                    .value;
  } catch (const vk::SystemError& e) {
    // If creation with cache fails, try without cache
    if (cache_handle != VK_NULL_HANDLE) {
      ERR("Pipeline creation with cache failed, retrying without cache: ", e.what());
      pipeline_ = c_.GetDevice()
                      .createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info)
                      .value;
    } else {
      throw;  // Re-throw if it already failed without cache
    }
  }
  
  c_.SetDbgName((uint64_t)(VkPipeline)pipeline_, vk::ObjectType::ePipeline,
                GetDbgName());
}

std::array<vk::PipelineShaderStageCreateInfo, 2> Pipeline::GetShaderStages(
    PipelineData& data, const std::string& vert_entry,
    const std::string& frag_entry,
    const std::vector<SpecConstInfo>& specialization_consts) {
  vert_shader_ver_ = vert_shader_.GetVersion();
  frag_shader_ver_ = frag_shader_.GetVersion();

  std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {
      vert_shader_.GetShaderStageInfo(vert_entry),
      frag_shader_.GetShaderStageInfo(frag_entry)};

  if (specialization_consts.empty()) return shader_stages;

  data.specialization_data.clear();
  data.specialization_entries.clear();
  data.specialization_entries.reserve(specialization_consts.size());

  uint32_t offset = 0;
  for (const auto& spec : specialization_consts) {
    data.specialization_entries.emplace_back(
        vk::SpecializationMapEntry{spec.constant_id, offset, spec.size});

    // Copy data
    const uint8_t* byte_data = static_cast<const uint8_t*>(spec.data);
    data.specialization_data.insert(data.specialization_data.end(), byte_data,
                                    byte_data + spec.size);

    offset += spec.size;
  }

  data.specialization_info.mapEntryCount = data.specialization_entries.size();
  data.specialization_info.pMapEntries = data.specialization_entries.data();
  data.specialization_info.dataSize = data.specialization_data.size();
  data.specialization_info.pData = data.specialization_data.data();

  shader_stages[1].pSpecializationInfo = &data.specialization_info;
  return shader_stages;
}

struct Vertex {
  glm::vec3 pos;
  glm::vec2 uv;

  glm::vec3 normal;
  glm::vec4 tangent;

  static vk::VertexInputBindingDescription GetBindingDesc();
  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDescs();
};

vk::VertexInputBindingDescription Vertex::GetBindingDesc() {
  vk::VertexInputBindingDescription binding_desc{};
  binding_desc.binding = 0;
  binding_desc.stride = sizeof(Vertex);
  binding_desc.inputRate = vk::VertexInputRate::eVertex;

  return binding_desc;
}

std::vector<vk::VertexInputAttributeDescription> Vertex::GetAttributeDescs() {
  std::vector<vk::VertexInputAttributeDescription> descs(4);

  // position
  descs[0].binding = 0;
  descs[0].location = 0;
  descs[0].format = vk::Format::eR32G32B32Sfloat;
  descs[0].offset = offsetof(Vertex, pos);

  // uv
  descs[1].binding = 0;
  descs[1].location = 1;
  descs[1].format = vk::Format::eR32G32Sfloat;
  descs[1].offset = offsetof(Vertex, uv);

  // normal
  descs[2].binding = 0;
  descs[2].location = 2;
  descs[2].format = vk::Format::eR32G32B32Sfloat;
  descs[2].offset = offsetof(Vertex, normal);

  // tan
  descs[3].binding = 0;
  descs[3].location = 3;
  descs[3].format = vk::Format::eR32G32B32A32Sfloat;
  descs[3].offset = offsetof(Vertex, tangent);

  return descs;
}

vk::PipelineVertexInputStateCreateInfo Pipeline::GetVertexInputState(
    PipelineData& data, bool use_vbo) const {
  vk::PipelineVertexInputStateCreateInfo vertex_input{};

  if (use_vbo) {
    data.binding_desc = Vertex::GetBindingDesc();
    data.attribute_descs = Vertex::GetAttributeDescs();

    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &data.binding_desc;
    vertex_input.vertexAttributeDescriptionCount = data.attribute_descs.size();
    vertex_input.pVertexAttributeDescriptions = data.attribute_descs.data();
  } else {
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.vertexAttributeDescriptionCount = 0;
  }

  return vertex_input;
}

vk::PipelineInputAssemblyStateCreateInfo Pipeline::GetInputAssemblyState()
    const {
  vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  return input_assembly;
}

vk::PipelineViewportStateCreateInfo Pipeline::GetViewportState() const {
  vk::PipelineViewportStateCreateInfo viewport{};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  return viewport;
}

vk::PipelineRasterizationStateCreateInfo Pipeline::GetRasterizationState()
    const {
  vk::PipelineRasterizationStateCreateInfo rasterization{};
  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = vk::PolygonMode::eFill;
  rasterization.cullMode = vk::CullModeFlagBits::eBack;
  rasterization.frontFace = vk::FrontFace::eCounterClockwise;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.lineWidth = 1.0f;
  return rasterization;
}

vk::PipelineMultisampleStateCreateInfo Pipeline::GetMultisampleState() const {
  vk::PipelineMultisampleStateCreateInfo multisample{};
  multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
  multisample.sampleShadingEnable = VK_FALSE;

  return multisample;
}

vk::PipelineDepthStencilStateCreateInfo Pipeline::GetDepthStencilState() const {
  vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.depthTestEnable = VK_FALSE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  return depth_stencil;
}

vk::PipelineColorBlendStateCreateInfo Pipeline::GetColorBlendState(
    PipelineData& data) const {
  data.color_attachments.resize(1);

  data.color_attachments[0].blendEnable = VK_FALSE;
  data.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  vk::PipelineColorBlendStateCreateInfo color_blend{};
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.attachmentCount = data.color_attachments.size();
  color_blend.pAttachments = data.color_attachments.data();

  return color_blend;
}

vk::PipelineDynamicStateCreateInfo Pipeline::GetDynamicState(
    PipelineData& data) const {
  data.dynamic_states = {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
      vk::DynamicState::eCullMode,
  };

  vk::PipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.dynamicStateCount = data.dynamic_states.size();
  dynamic_state.pDynamicStates = data.dynamic_states.data();
  return dynamic_state;
}

}  // namespace npr_graphics
