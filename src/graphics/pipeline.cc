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
                   const BasePass& render_pass)
    : ctx_{ctx}, cache_{cache}, render_pass_{render_pass} {}

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
                  vk::ObjectType::ePipelineLayout, GetDbgName() + "_layout");
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
                  GetDbgName());
}

/*
 * GBuffPipe
 */
GBuffPipe::GBuffPipe(const Context& ctx, const PipelineCache& cache,
                     const GBuffPass& render_pass, VertexShader& vert_shader,
                     FragmentShader& frag_shader,
                     vk::SampleCountFlagBits samples,
                     const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetCameraSets().GetLayout(), TextureArraySet(ctx_).GetLayout(),
       desc_pool.GetMaterialSets().GetLayout()},
      {});

  AddShader(vert_shader);
  AddShader(frag_shader,
            {MakeSpecConst(0, samples), MakeSpecConst(1, kMaxTextures)});

  AddVertexBinding(0, sizeof(Vertex));
  AddVertexBinding(1, sizeof(InstanceData), vk::VertexInputRate::eInstance);

  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
  AddVertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
  AddVertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(Vertex, normal));
  AddVertexAttribute(3, 0, vk::Format::eR32G32B32A32Sfloat,
                     offsetof(Vertex, tangent));

  for (int i = 0; i < 4; i++)
    AddVertexAttribute(4 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                       i * sizeof(glm::vec4));
  for (int i = 0; i < 3; i++)
    AddVertexAttribute(8 + i, 1, vk::Format::eR32G32B32Sfloat,
                       offsetof(InstanceData, normal) + i * sizeof(glm::vec3));

  state_.multisample.rasterizationSamples = samples;

  state_.dynamic_states = {vk::DynamicState::eViewport,
                           vk::DynamicState::eScissor,
                           vk::DynamicState::eCullMode};

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_TRUE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eReplace;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eAlways;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = BIT(1);
  state_.depth_stencil.front.reference = BIT(1);
  state_.depth_stencil.back = state_.depth_stencil.front;

  state_.color_attachments.resize(5);
  for (auto& att : state_.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }
  state_.color_attachments[4].colorWriteMask = vk::ColorComponentFlagBits::eR;

  BuildPipeline();
}

/*
 * AOPipe
 */
AOPipe::AOPipe(const Context& ctx, const PipelineCache& cache,
               const AOPass& render_pass, VertexShader& vert_shader,
               FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
               const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetCameraSets().GetLayout(),
       desc_pool.GetGBuffSets().GetLayout(), desc_pool.GetAOSet().GetLayout()},
      {MakePushConst<AOPushConst>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader,
            {MakeSpecConst(0, kAONoiseDim), MakeSpecConst(1, kAOKernelSize)});

  state_.multisample.rasterizationSamples = samples;

  state_.color_attachments.resize(1);
  state_.color_attachments[0].blendEnable = VK_FALSE;
  state_.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;

  state_.depth_stencil.depthTestEnable = VK_FALSE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eAlways;
  state_.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = 0;
  state_.depth_stencil.front.reference = BIT(1);
  state_.depth_stencil.back = state_.depth_stencil.front;

  BuildPipeline();
}

/*
 * AOBlurPipe
 */
AOBlurPipe::AOBlurPipe(const Context& ctx, const PipelineCache& cache,
                       const BlurPass& render_pass, VertexShader& vert_shader,
                       FragmentShader& frag_shader,
                       const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetAOResSets().GetLayout()},
      {MakePushConst<BlurPushConst>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader, {MakeSpecConst(0, kMaxGaussianRadius)});

  state_.color_attachments.resize(1);
  state_.color_attachments[0].blendEnable = VK_FALSE;
  state_.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;

  BuildPipeline();
}

/*
 * GlobLightPipe
 */
GlobLightPipe::GlobLightPipe(const Context& ctx, const PipelineCache& cache,
                             const GlobLightPass& render_pass,
                             VertexShader& vert_shader,
                             FragmentShader& frag_shader,
                             vk::SampleCountFlagBits samples,
                             const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout({desc_pool.GetAOResSets().GetLayout(),
                desc_pool.GetGBuffSets().GetLayout(),
                desc_pool.GetDirLightSets().GetLayout()},
               {});

  AddShader(vert_shader);
  AddShader(frag_shader,
            {MakeSpecConst(0, samples), MakeSpecConst(1, kMaxDirLights)});

  state_.multisample.rasterizationSamples = samples;

  state_.color_attachments.resize(1);
  state_.color_attachments[0].blendEnable = VK_FALSE;
  state_.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state_.depth_stencil.depthTestEnable = VK_FALSE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eAlways;
  state_.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = 0;
  state_.depth_stencil.front.reference = BIT(1);

  state_.depth_stencil.back = state_.depth_stencil.front;

  BuildPipeline();
}

/*
 * LocalLightPipe
 */
LocalLightPipe::LocalLightPipe(const Context& ctx, const PipelineCache& cache,
                               const LocalLightPass& render_pass,
                               VertexShader& vert_shader,
                               vk::SampleCountFlagBits samples,
                               const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetCameraSets().GetLayout()},
      {MakePushConst<LightPushConst>(vk::ShaderStageFlagBits::eVertex)});

  AddShader(vert_shader);

  AddVertexBinding(0, sizeof(LightVertex));
  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(LightVertex, pos));

  state_.multisample.rasterizationSamples = samples;
  state_.rasterization.cullMode = vk::CullModeFlagBits::eBack;

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eReplace;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eAlways;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = BIT(1);
  state_.depth_stencil.front.reference = BIT(1);
  state_.depth_stencil.back = state_.depth_stencil.front;

  state_.subpass = 0;

  BuildPipeline();
}

/*
 * PointLightPipe
 */
PointLightPipe::PointLightPipe(const Context& ctx, const PipelineCache& cache,
                               const LocalLightPass& render_pass,
                               VertexShader& vert_shader,
                               FragmentShader& frag_shader,
                               vk::SampleCountFlagBits samples,
                               const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetCameraSets().GetLayout(),
       desc_pool.GetGBuffSets().GetLayout(),
       desc_pool.GetPointLightSets().GetLayout()},
      {MakePushConst<LightPushConst>(vk::ShaderStageFlagBits::eVertex)});

  AddShader(vert_shader);
  AddShader(frag_shader, {MakeSpecConst(0, samples)});

  AddVertexBinding(0, sizeof(LightVertex));
  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(LightVertex, pos));

  state_.multisample.rasterizationSamples = samples;
  state_.rasterization.cullMode = vk::CullModeFlagBits::eFront;

  state_.color_attachments.resize(1);
  auto& att = state_.color_attachments[0];

  att.blendEnable = VK_TRUE;
  att.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  att.srcColorBlendFactor = vk::BlendFactor::eOne;
  att.dstColorBlendFactor = vk::BlendFactor::eOne;
  att.colorBlendOp = vk::BlendOp::eAdd;
  att.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  att.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  att.alphaBlendOp = vk::BlendOp::eAdd;

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = 0;
  state_.depth_stencil.front.reference = 0;
  state_.depth_stencil.back = state_.depth_stencil.front;

  state_.subpass = 1;

  BuildPipeline();
}

/*
 * SpotLightPipe
 */
SpotLightPipe::SpotLightPipe(const Context& ctx, const PipelineCache& cache,
                             const LocalLightPass& render_pass,
                             VertexShader& vert_shader,
                             FragmentShader& frag_shader,
                             vk::SampleCountFlagBits samples,
                             const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetCameraSets().GetLayout(),
       desc_pool.GetGBuffSets().GetLayout(),
       desc_pool.GetSpotLightSets().GetLayout()},
      {MakePushConst<LightPushConst>(vk::ShaderStageFlagBits::eVertex)});

  AddShader(vert_shader);
  AddShader(frag_shader, {MakeSpecConst(0, samples)});

  AddVertexBinding(0, sizeof(LightVertex));
  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(LightVertex, pos));

  state_.multisample.rasterizationSamples = samples;
  state_.rasterization.cullMode = vk::CullModeFlagBits::eFront;

  state_.color_attachments.resize(1);
  auto& att = state_.color_attachments[0];

  att.blendEnable = VK_TRUE;
  att.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  att.srcColorBlendFactor = vk::BlendFactor::eOne;
  att.dstColorBlendFactor = vk::BlendFactor::eOne;
  att.colorBlendOp = vk::BlendOp::eAdd;
  att.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  att.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  att.alphaBlendOp = vk::BlendOp::eAdd;

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
  state_.depth_stencil.stencilTestEnable = VK_TRUE;

  state_.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state_.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state_.depth_stencil.front.compareMask = BIT(1);
  state_.depth_stencil.front.writeMask = 0;
  state_.depth_stencil.front.reference = 0;
  state_.depth_stencil.back = state_.depth_stencil.front;

  state_.subpass = 1;

  BuildPipeline();
}

/*
 * ABuffFillPipe
 */
ABuffFillPipe::ABuffFillPipe(const Context& ctx, const PipelineCache& cache,
                             const ABuffPass& render_pass,
                             VertexShader& vert_shader,
                             FragmentShader& frag_shader,
                             vk::SampleCountFlagBits samples,
                             const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {
          desc_pool.GetCameraSets().GetLayout(),
          TextureArraySet(ctx_).GetLayout(),
          desc_pool.GetABufferSets().GetLayout(),
          desc_pool.GetMaterialSets().GetLayout(),
      },
      {MakePushConst<ABuffFillPushConst>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader,
            {MakeSpecConst(0, samples), MakeSpecConst(1, kMaxTextures)});

  AddVertexBinding(0, sizeof(Vertex));
  AddVertexBinding(1, sizeof(InstanceData), vk::VertexInputRate::eInstance);

  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
  AddVertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
  AddVertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(Vertex, normal));
  AddVertexAttribute(3, 0, vk::Format::eR32G32B32A32Sfloat,
                     offsetof(Vertex, tangent));

  for (int i = 0; i < 4; i++)
    AddVertexAttribute(4 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                       i * sizeof(glm::vec4));
  for (int i = 0; i < 3; i++)
    AddVertexAttribute(8 + i, 1, vk::Format::eR32G32B32Sfloat,
                       offsetof(InstanceData, normal) + i * sizeof(glm::vec3));

  state_.multisample.rasterizationSamples = samples;

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state_.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state_.depth_stencil.stencilTestEnable = VK_FALSE;

  state_.subpass = 0;

  BuildPipeline();
}

/*
 * ABuffResolvePipe
 */
ABuffResolvePipe::ABuffResolvePipe(const Context& ctx,
                                   const PipelineCache& cache,
                                   const ABuffPass& render_pass,
                                   VertexShader& vert_shader,
                                   FragmentShader& frag_shader,
                                   vk::SampleCountFlagBits samples,
                                   const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout({desc_pool.GetColorSets().GetLayout(),
                desc_pool.GetABufferSets().GetLayout()},
               {MakePushConst<uint32_t>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader, {MakeSpecConst(0, samples)});

  // present_color, bright_color
  state_.color_attachments.resize(2);
  for (auto& att : state_.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }

  state_.subpass = 1;

  BuildPipeline();
}

/*
 * WBoitAccPipe
 */
WBoitAccPipe::WBoitAccPipe(const Context& ctx, const PipelineCache& cache,
                           const WBoitPass& render_pass,
                           VertexShader& vert_shader,
                           FragmentShader& frag_shader,
                           vk::SampleCountFlagBits samples,
                           const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {
          desc_pool.GetCameraSets().GetLayout(),
          TextureArraySet(ctx_).GetLayout(),
          desc_pool.GetMaterialSets().GetLayout(),
      },
      {MakePushConst<ABuffFillPushConst>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader,
            {MakeSpecConst(0, samples), MakeSpecConst(1, kMaxTextures)});

  AddVertexBinding(0, sizeof(Vertex));
  AddVertexBinding(1, sizeof(InstanceData), vk::VertexInputRate::eInstance);

  AddVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
  AddVertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
  AddVertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat,
                     offsetof(Vertex, normal));
  AddVertexAttribute(3, 0, vk::Format::eR32G32B32A32Sfloat,
                     offsetof(Vertex, tangent));

  for (int i = 0; i < 4; i++)
    AddVertexAttribute(4 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                       i * sizeof(glm::vec4));
  for (int i = 0; i < 3; i++)
    AddVertexAttribute(8 + i, 1, vk::Format::eR32G32B32Sfloat,
                       offsetof(InstanceData, normal) + i * sizeof(glm::vec3));

  state_.multisample.rasterizationSamples = samples;

  state_.color_attachments.resize(2);

  state_.color_attachments[0].blendEnable = VK_TRUE;
  state_.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state_.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
  state_.color_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
  state_.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;
  state_.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
  state_.color_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
  state_.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

  state_.color_attachments[1].blendEnable = VK_TRUE;
  state_.color_attachments[1].colorWriteMask = vk::ColorComponentFlagBits::eR;

  state_.color_attachments[1].srcColorBlendFactor = vk::BlendFactor::eZero;
  state_.color_attachments[1].dstColorBlendFactor =
      vk::BlendFactor::eOneMinusSrcColor;
  state_.color_attachments[1].colorBlendOp = vk::BlendOp::eAdd;
  state_.color_attachments[1].srcAlphaBlendFactor = vk::BlendFactor::eZero;
  state_.color_attachments[1].dstAlphaBlendFactor =
      vk::BlendFactor::eOneMinusSrcColor;
  state_.color_attachments[1].alphaBlendOp = vk::BlendOp::eAdd;

  // for (auto& att : state_.color_attachments) {
  //   att.blendEnable = VK_TRUE;
  //   att.colorWriteMask =
  //       vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
  //       vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  //
  //   att.srcColorBlendFactor = vk::BlendFactor::eOne;
  //   att.dstColorBlendFactor = vk::BlendFactor::eOne;
  //   att.colorBlendOp = vk::BlendOp::eAdd;
  //
  //   att.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  //   att.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  //   att.alphaBlendOp = vk::BlendOp::eAdd;
  // }

  state_.depth_stencil.depthTestEnable = VK_TRUE;
  state_.depth_stencil.depthWriteEnable = VK_FALSE;
  state_.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state_.depth_stencil.stencilTestEnable = VK_FALSE;

  state_.subpass = 0;

  BuildPipeline();
}

/*
 * WBoitResolvePipe
 */
WBoitResolvePipe::WBoitResolvePipe(const Context& ctx,
                                   const PipelineCache& cache,
                                   const WBoitPass& render_pass,
                                   VertexShader& vert_shader,
                                   FragmentShader& frag_shader,
                                   const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout({desc_pool.GetColorSets().GetLayout(),
                desc_pool.GetWBoitInputSets().GetLayout()},
               {});

  AddShader(vert_shader);
  AddShader(frag_shader);

  // present_color, bright_color
  state_.color_attachments.resize(2);
  for (auto& att : state_.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }

  state_.subpass = 1;

  BuildPipeline();
}

/*
 * SwapPipe
 */
SwapPipe::SwapPipe(const Context& ctx, const PipelineCache& cache,
                   const SwapPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader, const DescriptorPool& desc_pool)
    : Pipeline(ctx, cache, render_pass) {
  CreateLayout(
      {desc_pool.GetPresentColorSets().GetLayout()},
      {MakePushConst<LoadPushConst>(vk::ShaderStageFlagBits::eFragment)});

  AddShader(vert_shader);
  AddShader(frag_shader);

  state_.color_attachments.resize(1);
  state_.color_attachments[0].blendEnable = VK_FALSE;
  state_.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state_.subpass = 0;

  BuildPipeline();
}

}  // namespace npr_graphics
