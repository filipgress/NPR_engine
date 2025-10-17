#include "pipeline.h"

namespace npr_graphics {

Pipeline::~Pipeline() {
  DestroyPipeline();
  if (layout_) c_.GetDevice().destroyPipelineLayout(layout_);
}

void Pipeline::DestroyPipeline() {
  if (pipeline_) {
    c_.GetDevice().destroyPipeline(pipeline_);
    pipeline_ = nullptr;
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
  DestroyPipeline();
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

  pipeline_ = c_.GetDevice()
                  .createGraphicsPipeline(cache_.GetCache(), pipeline_info)
                  .value;
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
