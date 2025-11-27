#include "pipe_manager.h"

namespace npr_graphics {
PipeManager::PipeManager(const Context& ctx, const Resources& resrc,
                         const DescriptorPool& desc_pool,
                         const PassManager& passes,
                         const ShaderManager& shaders)
    : pipe_cache_{ctx},
      gbuff_{ctx, pipe_cache_, passes.gbuff_, "gbuff_pipe"},
      ao_{ctx, pipe_cache_, passes.ao_, "ao_pipe"},
      ao_blur_{ctx, pipe_cache_, passes.ao_blur_h_, "ao_blur_pipe"},
      glob_light_{ctx, pipe_cache_, passes.glob_light_, "global_light_pipe"},
      local_light_{ctx, pipe_cache_, passes.local_light_, "local_light_pipe"},
      point_light_{ctx, pipe_cache_, passes.local_light_, "point_light_pipe"},
      spot_light_{ctx, pipe_cache_, passes.local_light_, "spot_light_pipe"},
      abuff_fill_{ctx, pipe_cache_, passes.abuff_, "abuff_fill_pipe"},
      abuff_res_{ctx, pipe_cache_, passes.abuff_, "abuff_resolve_pipe"},
      wboit_acc_{ctx, pipe_cache_, passes.wboit_, "wboit_acc_pipe"},
      wboit_res_{ctx, pipe_cache_, passes.wboit_, "wboit_resolve_pipe"},
      swap_{ctx, pipe_cache_, passes.swap_, "swap_pipe"} {
  BuildGBuff(shaders.gbuff_vert_, shaders.gbuff_frag_, resrc.GetProps().samples,
             desc_pool);
  BuildAO(shaders.quad_vert_, shaders.ao_frag_, resrc.GetProps().samples,
          desc_pool);
  BuildAOBlur(shaders.quad_vert_, shaders.blur_frag_, desc_pool);
  BuildGlobLight(shaders.quad_vert_, shaders.dir_light_frag_,
                 resrc.GetProps().samples, desc_pool);
  BuildLocalLight(shaders.light_vert_, resrc.GetProps().samples, desc_pool);
  BuildPointLight(shaders.light_vert_, shaders.point_light_frag_,
                  resrc.GetProps().samples, desc_pool);
  BuildSpotLight(shaders.light_vert_, shaders.spot_light_frag_,
                 resrc.GetProps().samples, desc_pool);
  BuildABuffFill(shaders.gbuff_vert_, shaders.abuff_fill_frag_,
                 resrc.GetProps().samples, desc_pool);
  BuildABuffRes(shaders.quad_vert_, shaders.abuff_res_frag_,
                resrc.GetProps().samples, desc_pool);
  BuildWBoitAcc(shaders.gbuff_vert_, shaders.wboit_acc_frag_,
                resrc.GetProps().samples, desc_pool);
  BuildWBoitRes(shaders.quad_vert_, shaders.wboit_res_frag_, desc_pool);
  BuildSwap(shaders.quad_vert_, shaders.swap_frag_, desc_pool);
}

void PipeManager::RebuildPipes() {
  if (!gbuff_.IsUpToDate()) gbuff_.BuildPipeline();
  if (!ao_.IsUpToDate()) ao_.BuildPipeline();
  if (!ao_blur_.IsUpToDate()) ao_blur_.BuildPipeline();
  if (!glob_light_.IsUpToDate()) glob_light_.BuildPipeline();
  if (!local_light_.IsUpToDate()) local_light_.BuildPipeline();
  if (!point_light_.IsUpToDate()) point_light_.BuildPipeline();
  if (!spot_light_.IsUpToDate()) spot_light_.BuildPipeline();
  if (!abuff_fill_.IsUpToDate()) abuff_fill_.BuildPipeline();
  if (!abuff_res_.IsUpToDate()) abuff_res_.BuildPipeline();
  if (!wboit_acc_.IsUpToDate()) wboit_acc_.BuildPipeline();
  if (!wboit_res_.IsUpToDate()) wboit_res_.BuildPipeline();
  if (!swap_.IsUpToDate()) swap_.BuildPipeline();
}

void PipeManager::BuildGBuff(const VertexShader& vert_shader,
                             const FragmentShader& frag_shader,
                             vk::SampleCountFlagBits samples,
                             const DescriptorPool& desc_pool) {
  gbuff_.CreateLayout({desc_pool.GetCameraSets().GetLayout(),
                       TextureArraySet(gbuff_.ctx_).GetLayout(),
                       desc_pool.GetMaterialSets().GetLayout()},
                      {});

  gbuff_.AddShader(vert_shader);
  gbuff_.AddShader(frag_shader, {Pipeline::MakeSpecConst(0, samples),
                                 Pipeline::MakeSpecConst(1, kMaxTextures)});

  gbuff_.AddObjectInstanceAttribs();

  auto& state = gbuff_.state_;
  state.multisample.rasterizationSamples = samples;

  state.dynamic_states = {vk::DynamicState::eViewport,
                          vk::DynamicState::eScissor,
                          vk::DynamicState::eCullMode};

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_TRUE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eReplace;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.compareOp = vk::CompareOp::eAlways;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = BIT(1);
  state.depth_stencil.front.reference = BIT(1);
  state.depth_stencil.back = state.depth_stencil.front;

  state.color_attachments.resize(5);
  for (auto& att : gbuff_.state_.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }
  state.color_attachments[4].colorWriteMask = vk::ColorComponentFlagBits::eR;

  state.subpass = 0;
  gbuff_.BuildPipeline();
}

void PipeManager::BuildAO(const VertexShader& vert_shader,
                          const FragmentShader& frag_shader,
                          vk::SampleCountFlagBits samples,
                          const DescriptorPool& desc_pool) {
  ao_.CreateLayout(
      {desc_pool.GetCameraSets().GetLayout(),
       desc_pool.GetGBuffSets().GetLayout(), desc_pool.GetAOSet().GetLayout()},
      {Pipeline::MakePushConst<AOPushConst>(
          vk::ShaderStageFlagBits::eFragment)});

  ao_.AddShader(vert_shader);
  ao_.AddShader(frag_shader, {Pipeline::MakeSpecConst(0, kAONoiseDim),
                              Pipeline::MakeSpecConst(1, kAOKernelSize)});

  auto& state = ao_.state_;
  state.multisample.rasterizationSamples = samples;

  state.color_attachments.resize(1);
  state.color_attachments[0].blendEnable = VK_FALSE;
  state.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;

  state.depth_stencil.depthTestEnable = VK_FALSE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eAlways;
  state.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = 0;
  state.depth_stencil.front.reference = BIT(1);
  state.depth_stencil.back = state.depth_stencil.front;

  state.subpass = 0;
  ao_.BuildPipeline();
}

void PipeManager::BuildAOBlur(const VertexShader& vert_shader,
                              const FragmentShader& frag_shader,
                              const DescriptorPool& desc_pool) {
  ao_blur_.CreateLayout({desc_pool.GetAOResSets().GetLayout()},
                        {Pipeline::MakePushConst<BlurPushConst>(
                            vk::ShaderStageFlagBits::eFragment)});

  ao_blur_.AddShader(vert_shader);
  ao_blur_.AddShader(frag_shader,
                     {Pipeline::MakeSpecConst(0, kMaxGaussianRadius)});

  auto& state = ao_blur_.state_;
  state.color_attachments.resize(1);
  state.color_attachments[0].blendEnable = VK_FALSE;
  state.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;
  state.subpass = 0;

  ao_blur_.BuildPipeline();
}

void PipeManager::BuildGlobLight(const VertexShader& vert_shader,
                                 const FragmentShader& frag_shader,
                                 vk::SampleCountFlagBits samples,
                                 const DescriptorPool& desc_pool) {
  glob_light_.CreateLayout({desc_pool.GetAOResSets().GetLayout(),
                            desc_pool.GetGBuffSets().GetLayout(),
                            desc_pool.GetDirLightSets().GetLayout()},
                           {});

  glob_light_.AddShader(vert_shader);
  glob_light_.AddShader(frag_shader,
                        {Pipeline::MakeSpecConst(0, samples),
                         Pipeline::MakeSpecConst(1, kMaxDirLights)});

  auto& state = glob_light_.state_;
  state.multisample.rasterizationSamples = samples;

  state.color_attachments.resize(1);
  state.color_attachments[0].blendEnable = VK_FALSE;
  state.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state.depth_stencil.depthTestEnable = VK_FALSE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eAlways;
  state.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = 0;
  state.depth_stencil.front.reference = BIT(1);

  state.depth_stencil.back = state.depth_stencil.front;

  state.subpass = 0;
  glob_light_.BuildPipeline();
}

void PipeManager::BuildLocalLight(const VertexShader& vert_shader,
                                  vk::SampleCountFlagBits samples,
                                  const DescriptorPool& desc_pool) {
  local_light_.CreateLayout({desc_pool.GetCameraSets().GetLayout()},
                            {Pipeline::MakePushConst<LightPushConst>(
                                vk::ShaderStageFlagBits::eVertex)});

  local_light_.AddShader(vert_shader);

  local_light_.AddLightVertexAttribs();

  auto& state = local_light_.state_;
  state.multisample.rasterizationSamples = samples;
  state.rasterization.cullMode = vk::CullModeFlagBits::eBack;

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eReplace;
  state.depth_stencil.front.compareOp = vk::CompareOp::eAlways;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = BIT(1);
  state.depth_stencil.front.reference = BIT(1);
  state.depth_stencil.back = state.depth_stencil.front;

  state.subpass = 0;
  local_light_.BuildPipeline();
}

void PipeManager::BuildPointLight(const VertexShader& vert_shader,
                                  const FragmentShader& frag_shader,
                                  vk::SampleCountFlagBits samples,
                                  const DescriptorPool& desc_pool) {
  point_light_.CreateLayout({desc_pool.GetCameraSets().GetLayout(),
                             desc_pool.GetGBuffSets().GetLayout(),
                             desc_pool.GetPointLightSets().GetLayout()},
                            {Pipeline::MakePushConst<LightPushConst>(
                                vk::ShaderStageFlagBits::eVertex)});

  point_light_.AddShader(vert_shader);
  point_light_.AddShader(frag_shader, {Pipeline::MakeSpecConst(0, samples)});

  point_light_.AddLightVertexAttribs();

  auto& state = point_light_.state_;
  state.multisample.rasterizationSamples = samples;
  state.rasterization.cullMode = vk::CullModeFlagBits::eFront;

  state.color_attachments.resize(1);
  auto& att = state.color_attachments[0];

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

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = 0;
  state.depth_stencil.front.reference = 0;
  state.depth_stencil.back = state.depth_stencil.front;

  state.subpass = 1;
  point_light_.BuildPipeline();
}

void PipeManager::BuildSpotLight(const VertexShader& vert_shader,
                                 const FragmentShader& frag_shader,
                                 vk::SampleCountFlagBits samples,
                                 const DescriptorPool& desc_pool) {
  spot_light_.CreateLayout({desc_pool.GetCameraSets().GetLayout(),
                            desc_pool.GetGBuffSets().GetLayout(),
                            desc_pool.GetSpotLightSets().GetLayout()},
                           {Pipeline::MakePushConst<LightPushConst>(
                               vk::ShaderStageFlagBits::eVertex)});

  spot_light_.AddShader(vert_shader);
  spot_light_.AddShader(frag_shader, {Pipeline::MakeSpecConst(0, samples)});

  spot_light_.AddLightVertexAttribs();

  auto& state = spot_light_.state_;

  state.multisample.rasterizationSamples = samples;
  state.rasterization.cullMode = vk::CullModeFlagBits::eFront;

  state.color_attachments.resize(1);
  auto& att = state.color_attachments[0];

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

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
  state.depth_stencil.stencilTestEnable = VK_TRUE;

  state.depth_stencil.front.failOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.passOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.depthFailOp = vk::StencilOp::eKeep;
  state.depth_stencil.front.compareOp = vk::CompareOp::eEqual;
  state.depth_stencil.front.compareMask = BIT(1);
  state.depth_stencil.front.writeMask = 0;
  state.depth_stencil.front.reference = 0;
  state.depth_stencil.back = state.depth_stencil.front;

  state.subpass = 1;
  spot_light_.BuildPipeline();
}

void PipeManager::BuildABuffFill(const VertexShader& vert_shader,
                                 const FragmentShader& frag_shader,
                                 vk::SampleCountFlagBits samples,
                                 const DescriptorPool& desc_pool) {
  abuff_fill_.CreateLayout(
      {
          desc_pool.GetCameraSets().GetLayout(),
          TextureArraySet(abuff_fill_.ctx_).GetLayout(),
          desc_pool.GetABufferSets().GetLayout(),
          desc_pool.GetMaterialSets().GetLayout(),
      },
      {Pipeline::MakePushConst<ABuffFillPushConst>(
          vk::ShaderStageFlagBits::eFragment)});

  abuff_fill_.AddShader(vert_shader);
  abuff_fill_.AddShader(frag_shader,
                        {Pipeline::MakeSpecConst(0, samples),
                         Pipeline::MakeSpecConst(1, kMaxTextures)});

  abuff_fill_.AddObjectInstanceAttribs();

  auto& state = abuff_fill_.state_;
  state.multisample.rasterizationSamples = samples;

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state.depth_stencil.depthBoundsTestEnable = VK_FALSE;
  state.depth_stencil.stencilTestEnable = VK_FALSE;

  state.subpass = 0;
  abuff_fill_.BuildPipeline();
}

void PipeManager::BuildABuffRes(const VertexShader& vert_shader,
                                const FragmentShader& frag_shader,
                                vk::SampleCountFlagBits samples,
                                const DescriptorPool& desc_pool) {
  abuff_res_.CreateLayout({desc_pool.GetColorSets().GetLayout(),
                           desc_pool.GetABufferSets().GetLayout()},
                          {Pipeline::MakePushConst<ABuffResPushConst>(
                              vk::ShaderStageFlagBits::eFragment)});

  abuff_res_.AddShader(vert_shader);
  abuff_res_.AddShader(frag_shader,
                       {Pipeline::MakeSpecConst(0, samples),
                        Pipeline::MakeSpecConst(1, kABuffMaxSortedNodes)});

  // present_color, bright_color
  auto& state = abuff_res_.state_;
  state.color_attachments.resize(2);
  for (auto& att : state.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }

  state.subpass = 1;
  abuff_res_.BuildPipeline();
}

void PipeManager::BuildWBoitAcc(const VertexShader& vert_shader,
                                const FragmentShader& frag_shader,
                                vk::SampleCountFlagBits samples,
                                const DescriptorPool& desc_pool) {
  wboit_acc_.CreateLayout(
      {
          desc_pool.GetCameraSets().GetLayout(),
          TextureArraySet(wboit_acc_.ctx_).GetLayout(),
          desc_pool.GetMaterialSets().GetLayout(),
      },
      {Pipeline::MakePushConst<WBoitPushConst>(
          vk::ShaderStageFlagBits::eFragment)});

  wboit_acc_.AddShader(vert_shader);
  wboit_acc_.AddShader(frag_shader, {Pipeline::MakeSpecConst(0, samples),
                                     Pipeline::MakeSpecConst(1, kMaxTextures)});

  wboit_acc_.AddObjectInstanceAttribs();

  auto& state = wboit_acc_.state_;

  state.multisample.rasterizationSamples = samples;

  state.color_attachments.resize(2);

  state.color_attachments[0].blendEnable = VK_TRUE;
  state.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
  state.color_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
  state.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;
  state.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
  state.color_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
  state.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

  state.color_attachments[1].blendEnable = VK_TRUE;
  state.color_attachments[1].colorWriteMask = vk::ColorComponentFlagBits::eR;

  state.color_attachments[1].srcColorBlendFactor = vk::BlendFactor::eZero;
  state.color_attachments[1].dstColorBlendFactor =
      vk::BlendFactor::eOneMinusSrcColor;
  state.color_attachments[1].colorBlendOp = vk::BlendOp::eAdd;
  state.color_attachments[1].srcAlphaBlendFactor = vk::BlendFactor::eZero;
  state.color_attachments[1].dstAlphaBlendFactor =
      vk::BlendFactor::eOneMinusSrcColor;
  state.color_attachments[1].alphaBlendOp = vk::BlendOp::eAdd;

  state.depth_stencil.depthTestEnable = VK_TRUE;
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  state.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
  state.depth_stencil.stencilTestEnable = VK_FALSE;

  state.subpass = 0;
  wboit_acc_.BuildPipeline();
}

void PipeManager::BuildWBoitRes(const VertexShader& vert_shader,
                                const FragmentShader& frag_shader,
                                const DescriptorPool& desc_pool) {
  wboit_res_.CreateLayout({desc_pool.GetColorSets().GetLayout(),
                           desc_pool.GetWBoitInputSets().GetLayout()},
                          {});

  wboit_res_.AddShader(vert_shader);
  wboit_res_.AddShader(frag_shader);

  // present_color, bright_color
  auto& state = wboit_res_.state_;

  state.color_attachments.resize(2);
  for (auto& att : state.color_attachments) {
    att.blendEnable = VK_FALSE;
    att.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }

  state.subpass = 1;
  wboit_res_.BuildPipeline();
}

void PipeManager::BuildSwap(const VertexShader& vert_shader,
                            const FragmentShader& frag_shader,
                            const DescriptorPool& desc_pool) {
  swap_.CreateLayout({desc_pool.GetPresentColorSets().GetLayout()},
                     {Pipeline::MakePushConst<LoadPushConst>(
                         vk::ShaderStageFlagBits::eFragment)});

  swap_.AddShader(vert_shader);
  swap_.AddShader(frag_shader);

  auto& state = swap_.state_;
  state.color_attachments.resize(1);
  state.color_attachments[0].blendEnable = VK_FALSE;
  state.color_attachments[0].colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  state.subpass = 0;
  swap_.BuildPipeline();
}

}  // namespace npr_graphics
