#include "pipeline.h"
#include "buffer.h"

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

    offset += npr_core::Align(pc.size, 4);
  }

  pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
  pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

  layout_ = c_.GetDevice().createPipelineLayout(pipeline_layout_info);
  c_.SetDbgName((uint64_t)(VkPipelineLayout)layout_,
                vk::ObjectType::ePipelineLayout, GetDbgName() + "_layout");
}

void Pipeline::CreatePipeline(size_t subpass, const std::string& vert_entry,
                              const std::string& frag_entry,
                              const std::vector<SpecConstInfo>& spec_consts) {
  DestroyPipeline();

  auto shader_stages = GetShaderStages(vert_entry, frag_entry, spec_consts);
  auto vertex_input = GetVertexInputState();
  auto input_assembly = GetInputAssemblyState();
  auto viewport = GetViewportState();
  auto rasterization = GetRasterizationState();
  auto multisample = GetMultisampleState();
  auto depth_stencil = GetDepthStencilState();
  auto color_blend = GetColorBlendState();
  auto dynamic_state = GetDynamicState();

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
    const std::string& vert_entry, const std::string& frag_entry,
    const std::vector<SpecConstInfo>& specialization_consts) {
  vert_shader_ver_ = vert_shader_.GetVersion();
  frag_shader_ver_ = frag_shader_.GetVersion();

  std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {
      vert_shader_.GetShaderStageInfo(vert_entry),
      frag_shader_.GetShaderStageInfo(frag_entry)};

  if (specialization_consts.empty()) return shader_stages;
  if (data_.specialization_data.empty()) {
    data_.specialization_entries.reserve(specialization_consts.size());
    uint32_t offset = 0;
    for (const auto& spec : specialization_consts) {
      data_.specialization_entries.emplace_back(
          vk::SpecializationMapEntry{spec.constant_id, offset, spec.size});

      // Copy data
      const uint8_t* byte_data = static_cast<const uint8_t*>(spec.data);
      data_.specialization_data.insert(data_.specialization_data.end(),
                                       byte_data, byte_data + spec.size);

      offset += spec.size;
    }

    data_.specialization_info.mapEntryCount =
        data_.specialization_entries.size();
    data_.specialization_info.pMapEntries = data_.specialization_entries.data();
    data_.specialization_info.dataSize = data_.specialization_data.size();
    data_.specialization_info.pData = data_.specialization_data.data();
  }

  shader_stages[1].pSpecializationInfo = &data_.specialization_info;
  return shader_stages;
}

vk::PipelineVertexInputStateCreateInfo Pipeline::GetVertexInputState() {
  vk::PipelineVertexInputStateCreateInfo vertex_input{};

  if (data_.use_vbo) {
    // Vertex binding
    data_.binding_descs[0].binding = 0;
    data_.binding_descs[0].stride = sizeof(Vertex);
    data_.binding_descs[0].inputRate = vk::VertexInputRate::eVertex;

    // Instance binding
    data_.binding_descs[1].binding = 1;
    data_.binding_descs[1].stride = sizeof(InstanceData);
    data_.binding_descs[1].inputRate = vk::VertexInputRate::eInstance;

    data_.attr_descs.resize(11);

    // position
    data_.attr_descs[0].binding = 0;
    data_.attr_descs[0].location = 0;
    data_.attr_descs[0].format = vk::Format::eR32G32B32Sfloat;
    data_.attr_descs[0].offset = offsetof(Vertex, pos);

    // uv
    data_.attr_descs[1].binding = 0;
    data_.attr_descs[1].location = 1;
    data_.attr_descs[1].format = vk::Format::eR32G32Sfloat;
    data_.attr_descs[1].offset = offsetof(Vertex, uv);

    // normal
    data_.attr_descs[2].binding = 0;
    data_.attr_descs[2].location = 2;
    data_.attr_descs[2].format = vk::Format::eR32G32B32Sfloat;
    data_.attr_descs[2].offset = offsetof(Vertex, normal);

    // tan
    data_.attr_descs[3].binding = 0;
    data_.attr_descs[3].location = 3;
    data_.attr_descs[3].format = vk::Format::eR32G32B32A32Sfloat;
    data_.attr_descs[3].offset = offsetof(Vertex, tangent);

    // model matrix (4 vec4s)
    for (int i = 0; i < 4; i++) {
      data_.attr_descs[4 + i].binding = 1;
      data_.attr_descs[4 + i].location = 4 + i;
      data_.attr_descs[4 + i].format = vk::Format::eR32G32B32A32Sfloat;
      data_.attr_descs[4 + i].offset = sizeof(glm::vec4) * i;
    }

    // normal matrix (3 vec3s)
    for (int i = 0; i < 3; i++) {
      data_.attr_descs[8 + i].binding = 1;
      data_.attr_descs[8 + i].location = 8 + i;
      data_.attr_descs[8 + i].format = vk::Format::eR32G32B32Sfloat;
      data_.attr_descs[8 + i].offset =
          offsetof(InstanceData, normal) + sizeof(glm::vec3) * i;
    }

    vertex_input.vertexBindingDescriptionCount = data_.binding_descs.size();
    vertex_input.pVertexBindingDescriptions = data_.binding_descs.data();
    vertex_input.vertexAttributeDescriptionCount = data_.attr_descs.size();
    vertex_input.pVertexAttributeDescriptions = data_.attr_descs.data();
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
  rasterization.cullMode = data_.cull_mode;
  rasterization.frontFace = vk::FrontFace::eCounterClockwise;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.lineWidth = 1.0f;
  return rasterization;
}

vk::PipelineMultisampleStateCreateInfo Pipeline::GetMultisampleState() const {
  vk::PipelineMultisampleStateCreateInfo multisample{};
  multisample.rasterizationSamples = data_.samples;
  multisample.sampleShadingEnable = VK_FALSE;
  // multisample.sampleShadingEnable =
  //     data_.samples == vk::SampleCountFlagBits::e1 ? VK_FALSE : VK_TRUE;
  // multisample.minSampleShading = 1.0f;

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

vk::PipelineColorBlendStateCreateInfo Pipeline::GetColorBlendState() const {
  vk::PipelineColorBlendStateCreateInfo color_blend{};
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.attachmentCount = data_.color_attachments.size();
  color_blend.pAttachments = data_.color_attachments.data();

  return color_blend;
}

vk::PipelineDynamicStateCreateInfo Pipeline::GetDynamicState() const {
  vk::PipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.dynamicStateCount = data_.dynamic_states.size();
  dynamic_state.pDynamicStates = data_.dynamic_states.data();
  return dynamic_state;
}

/*
 * GBuffPipe
 */
vk::PipelineDepthStencilStateCreateInfo GBuffPipe::GetDepthStencilState()
    const {
  vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  depth_stencil.stencilTestEnable = VK_TRUE;

  depth_stencil.front.failOp = vk::StencilOp::eKeep;
  depth_stencil.front.passOp = vk::StencilOp::eReplace;
  depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  depth_stencil.front.compareOp = vk::CompareOp::eAlways;
  depth_stencil.front.compareMask = BIT(1);
  depth_stencil.front.writeMask = BIT(1);
  depth_stencil.front.reference = BIT(1);

  depth_stencil.back = depth_stencil.front;

  return depth_stencil;
}

/*
 * ABuffFillPipe
 */
vk::PipelineDepthStencilStateCreateInfo ABuffFillPipe::GetDepthStencilState()
    const {
  vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  return depth_stencil;
}

}  // namespace npr_graphics
