#include "pipeline.h"

namespace npr_graphics {

PipelineState::PipelineState() {
  input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = vk::PolygonMode::eFill;
  rasterization.cullMode = vk::CullModeFlagBits::eNone;
  rasterization.frontFace = vk::FrontFace::eCounterClockwise;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.lineWidth = 1.0f;

  multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
  multisample.sampleShadingEnable = VK_FALSE;

  depth_stencil.depthTestEnable = VK_FALSE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
}

Pipeline::Pipeline(const Context& ctx, const PipelineCache& cache,
                   const BasePass& render_pass, const std::string& dbg_name)
    : ctx_{ctx},
      cache_{cache},
      render_pass_{render_pass},
      dbg_name_{dbg_name} {}

Pipeline::~Pipeline() {
  if (pipeline_) ctx_.GetDevice().destroyPipeline(pipeline_);
  if (layout_) ctx_.GetDevice().destroyPipelineLayout(layout_);
}

bool Pipeline::IsUpToDate() const {
  for (const auto& shader_info : state_.shaders)
    if (shader_info.built_ver != shader_info.shader.GetVer()) return false;
  return true;
}

void Pipeline::CreateLayout(
    const std::vector<vk::DescriptorSetLayout>& set_layouts,
    const std::vector<PushConstInfo>& push_consts) {
  vk::PipelineLayoutCreateInfo info{};

  info.setLayoutCount = set_layouts.size();
  info.pSetLayouts = set_layouts.data();

  std::vector<vk::PushConstantRange> ranges{};
  ranges.reserve(push_consts.size());

  size_t offset{0};
  for (const auto& pc : push_consts) {
    if (pc.size == 0) continue;

    vk::PushConstantRange range{};
    range.stageFlags = pc.stage;
    range.offset = offset;
    range.size = pc.size;
    ranges.push_back(range);

    offset += npr_core::Align(pc.size, 4);
  }

  info.pushConstantRangeCount = ranges.size();
  info.pPushConstantRanges = ranges.data();

  layout_ = ctx_.GetDevice().createPipelineLayout(info);
  ctx_.SetDbgName((uint64_t)(VkPipelineLayout)layout_,
                  vk::ObjectType::ePipelineLayout, dbg_name_ + "_layout");
}

void Pipeline::BuildPipeline() {
  if (pipeline_) {
    ctx_.GetDevice().destroyPipeline(pipeline_);
    pipeline_ = nullptr;
  }

  std::vector<vk::PipelineShaderStageCreateInfo> stages{};
  std::list<vk::SpecializationInfo> spec_infos;
  std::list<std::vector<vk::SpecializationMapEntry>> spec_entries_list;
  std::list<std::vector<uint8_t>> spec_data_list;

  for (auto& shader_info : state_.shaders) {
    auto stage_info = shader_info.shader.GetShaderStageInfo(shader_info.entry);
    shader_info.built_ver = shader_info.shader.GetVer();

    if (!shader_info.spec_consts.empty()) {
      spec_entries_list.emplace_back();
      spec_data_list.emplace_back();

      auto& entries = spec_entries_list.back();
      auto& data = spec_data_list.back();

      entries.reserve(shader_info.spec_consts.size());

      uint32_t offset = 0;
      for (const auto& spec : shader_info.spec_consts) {
        entries.emplace_back(
            vk::SpecializationMapEntry{spec.constant_id, offset, spec.size});

        // copy data
        const uint8_t* byte_data = static_cast<const uint8_t*>(spec.data);
        data.insert(data.end(), byte_data, byte_data + spec.size);

        offset += spec.size;
      }

      spec_infos.emplace_back(static_cast<uint32_t>(entries.size()),
                              entries.data(), static_cast<size_t>(data.size()),
                              data.data());
      stage_info.pSpecializationInfo = &spec_infos.back();
    }

    stages.push_back(stage_info);
  }

  vk::PipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.vertexBindingDescriptionCount = state_.vertex_bindings.size();
  vertex_input.pVertexBindingDescriptions = state_.vertex_bindings.data();
  vertex_input.vertexAttributeDescriptionCount = state_.vertex_attribs.size();
  vertex_input.pVertexAttributeDescriptions = state_.vertex_attribs.data();

  vk::PipelineColorBlendStateCreateInfo color_blend{};
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.attachmentCount = state_.color_attachments.size();
  color_blend.pAttachments = state_.color_attachments.data();

  vk::PipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.dynamicStateCount = state_.dynamic_states.size();
  dynamic_state.pDynamicStates = state_.dynamic_states.data();

  vk::GraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.stageCount = stages.size();
  pipeline_info.pStages = stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &state_.input_assembly;
  pipeline_info.pViewportState = &state_.viewport;
  pipeline_info.pRasterizationState = &state_.rasterization;
  pipeline_info.pMultisampleState = &state_.multisample;
  pipeline_info.pDepthStencilState = &state_.depth_stencil;
  pipeline_info.pColorBlendState = &color_blend;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = layout_;
  pipeline_info.renderPass = render_pass_.GetRenderPass();
  pipeline_info.subpass = state_.subpass;

  pipeline_ = ctx_.GetDevice()
                  .createGraphicsPipeline(cache_.GetCache(), pipeline_info)
                  .value;
  ctx_.SetDbgName((uint64_t)(VkPipeline)pipeline_, vk::ObjectType::ePipeline,
                  dbg_name_);
}

void Pipeline::AddLightVertexAttribs() {
  AddVertexBinding(0, sizeof(LightVertex));
  AddVertexAttrib(0, 0, vk::Format::eR32G32B32Sfloat,
                  offsetof(LightVertex, pos));
}

void Pipeline::AddObjectInstanceAttribs() {
  AddVertexBinding(0, sizeof(Vertex));
  AddVertexBinding(1, sizeof(InstanceData), vk::VertexInputRate::eInstance);

  // vertex attributes
  AddVertexAttrib(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
  AddVertexAttrib(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
  AddVertexAttrib(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal));
  AddVertexAttrib(3, 0, vk::Format::eR32G32B32A32Sfloat,
                  offsetof(Vertex, tangent));

  // instance attributes

  // mat4 model
  for (int i = 0; i < 4; i++)
    AddVertexAttrib(4 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                    offsetof(InstanceData, model) + i * sizeof(glm::vec4));

  // mat4 prev_model
  for (int i = 0; i < 4; i++)
    AddVertexAttrib(8 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                    offsetof(InstanceData, prev_model) + i * sizeof(glm::vec4));

  // mat3 normal
  for (int i = 0; i < 3; i++)
    AddVertexAttrib(12 + i, 1, vk::Format::eR32G32B32Sfloat,
                    offsetof(InstanceData, normal) + i * sizeof(glm::vec3));
}

}  // namespace npr_graphics
