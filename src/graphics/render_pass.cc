#include "render_pass.h"

namespace npr_graphics {
BasePass::~BasePass() {
  DestroyFramebuffers();
  if (render_pass_) ctx_.GetDevice().destroyRenderPass(render_pass_);
}

void BasePass::DestroyFramebuffers() {
  if (framebuffers_.empty()) return;

  for (const auto& framebuffer : framebuffers_)
    ctx_.GetDevice().destroyFramebuffer(framebuffer);
  framebuffers_.clear();
}

vk::RenderPassBeginInfo BasePass::BeginInfo(uint frame_idx,
                                            vk::Extent2D extent) const {
  vk::RenderPassBeginInfo begin_info{};

  begin_info.renderPass = render_pass_;
  begin_info.framebuffer = framebuffers_[frame_idx];
  begin_info.renderArea.offset = vk::Offset2D{0, 0};
  begin_info.renderArea.extent = extent;
  begin_info.clearValueCount = clear_values_.size();
  begin_info.pClearValues = clear_values_.data();

  return begin_info;
}

void BasePass::CreateRenderPass() {
  auto attachments = GetAttachments();
  auto dependencies = GetDependencies();
  auto subpasses = GetSubpasses();

  vk::RenderPassCreateInfo render_pass_info{};
  render_pass_info.attachmentCount = attachments.size();
  render_pass_info.pAttachments = attachments.data();
  render_pass_info.subpassCount = subpasses.size();
  render_pass_info.pSubpasses = subpasses.data();
  render_pass_info.dependencyCount = dependencies.size();
  render_pass_info.pDependencies = dependencies.data();

  render_pass_ = ctx_.GetDevice().createRenderPass(render_pass_info);
  ctx_.SetDbgName((uint64_t)(VkRenderPass)render_pass_,
                  vk::ObjectType::eRenderPass, GetDbgName());
}

void RenderPass::CreateFramebuffers() {
  DestroyFramebuffers();

  auto& resrc = resrc_.GetResrc();
  auto extent = resrc_.GetProps().extent;

  framebuffers_.reserve(resrc.size());
  for (size_t i = 0; i < resrc.size(); i++) {
    std::vector<vk::ImageView> attachments = GetAttachmentViews(i);

    vk::FramebufferCreateInfo framebuff_info{};
    framebuff_info.renderPass = render_pass_;
    framebuff_info.attachmentCount = attachments.size();
    framebuff_info.pAttachments = attachments.data();
    framebuff_info.width = extent.width;
    framebuff_info.height = extent.height;
    framebuff_info.layers = 1;

    framebuffers_.push_back(ctx_.GetDevice().createFramebuffer(framebuff_info));
  }
}

/*
 * GBuffPass
 */
void GBuffPass::SetClearValues() {
  clear_values_.resize(7);

  // albedo_ms, emissive_ms, position_ms, normal_ms, ds_ms,
  // coverage_ms, coverage_res
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
  clear_values_[1].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
  clear_values_[2].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
  clear_values_[3].color = std::array<float, 4>{0.0f, 0.0f, -1.0f, 0.0f};
  clear_values_[4].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
  clear_values_[5].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
  clear_values_[6].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
}

std::vector<vk::AttachmentDescription> GBuffPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachs(7);

  // albedo_ms
  attachs[0].format = props.albedo_format;
  attachs[0].samples = props.samples;
  attachs[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[0].initialLayout = vk::ImageLayout::eUndefined;
  attachs[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // emissive_ms
  attachs[1].format = props.emissive_format;
  attachs[1].samples = props.samples;
  attachs[1].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[1].initialLayout = vk::ImageLayout::eUndefined;
  attachs[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // position_ms
  attachs[2].format = props.position_format;
  attachs[2].samples = props.samples;
  attachs[2].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[2].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[2].initialLayout = vk::ImageLayout::eUndefined;
  attachs[2].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // normal_ms
  attachs[3].format = props.normal_format;
  attachs[3].samples = props.samples;
  attachs[3].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[3].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[3].initialLayout = vk::ImageLayout::eUndefined;
  attachs[3].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // ds_ms
  attachs[4].format = props.ds_format;
  attachs[4].samples = props.samples;
  attachs[4].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[4].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[4].stencilLoadOp = vk::AttachmentLoadOp::eClear;
  attachs[4].stencilStoreOp = vk::AttachmentStoreOp::eStore;
  attachs[4].initialLayout = vk::ImageLayout::eUndefined;
  attachs[4].finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

  // coverage_ms
  attachs[5].format = props.coverage_format;
  attachs[5].samples = props.samples;
  attachs[5].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[5].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachs[5].initialLayout = vk::ImageLayout::eUndefined;
  attachs[5].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // coverage_res
  attachs[6].format = props.coverage_format;
  attachs[6].samples = vk::SampleCountFlagBits::e1;
  attachs[6].loadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[6].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[6].initialLayout = vk::ImageLayout::eUndefined;
  attachs[6].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  return attachs;
}

std::vector<vk::SubpassDependency> GBuffPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  {
    // ensure images are ready to be written to
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    deps[0].srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
    deps[0].srcAccessMask = vk::AccessFlagBits::eNone;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests;
    deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    // wait for all writes (color, resolve, depth) to be finished
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;

    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests;
    deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead |
                            vk::AccessFlagBits::eDepthStencilAttachmentRead |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> GBuffPass::GetSubpasses() {
  color_refs_[0] = {0, vk::ImageLayout::eColorAttachmentOptimal};  // albedo
  color_refs_[1] = {1, vk::ImageLayout::eColorAttachmentOptimal};  // emissive
  color_refs_[2] = {2, vk::ImageLayout::eColorAttachmentOptimal};  // position
  color_refs_[3] = {3, vk::ImageLayout::eColorAttachmentOptimal};  // normal
  color_refs_[4] = {5, vk::ImageLayout::eColorAttachmentOptimal};  // coverage

  resolve_refs_[0] = {VK_ATTACHMENT_UNUSED, vk::ImageLayout::eUndefined};
  resolve_refs_[1] = {VK_ATTACHMENT_UNUSED, vk::ImageLayout::eUndefined};
  resolve_refs_[2] = {VK_ATTACHMENT_UNUSED, vk::ImageLayout::eUndefined};
  resolve_refs_[3] = {VK_ATTACHMENT_UNUSED, vk::ImageLayout::eUndefined};
  resolve_refs_[4] = {6, vk::ImageLayout::eColorAttachmentOptimal};

  depth_ref_ = {4, vk::ImageLayout::eDepthStencilAttachmentOptimal};

  vk::SubpassDescription subpass;
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = color_refs_.size();
  subpass.pColorAttachments = color_refs_.data();
  subpass.pResolveAttachments = resolve_refs_.data();
  subpass.pDepthStencilAttachment = &depth_ref_;

  return {subpass};
}

std::vector<vk::ImageView> GBuffPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.albedo_metallic_ms || !resrc.emissive_roughness_ms ||
      !resrc.position_ms || !resrc.normal_ms || !resrc.ds_ms ||
      !resrc.coverage_ms || !resrc.coverage_res) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {resrc.albedo_metallic_ms->GetImageView(),
          resrc.emissive_roughness_ms->GetImageView(),
          resrc.position_ms->GetImageView(),
          resrc.normal_ms->GetImageView(),
          resrc.ds_ms->GetImageView(),
          resrc.coverage_ms->GetImageView(),
          resrc.coverage_res->GetImageView()};
}

/*
 * AOPass
 */
void AOPass::SetClearValues() {
  clear_values_.resize(3);

  // ao_ms
  clear_values_[0].color = std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f};
}

std::vector<vk::AttachmentDescription> AOPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(3);

  // ao_ms
  attachments[0].format = props.ao_format;
  attachments[0].samples = props.samples;
  attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[0].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // ao_res
  attachments[1].format = props.ao_format;
  attachments[1].samples = vk::SampleCountFlagBits::e1;
  attachments[1].loadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[1].initialLayout = vk::ImageLayout::eUndefined;
  attachments[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // ds_ms (from GBuffPass)
  attachments[2].format = props.ds_format;
  attachments[2].samples = props.samples;
  attachments[2].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[2].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eLoad;
  attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eStore;
  attachments[2].initialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
  attachments[2].finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> AOPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    // wait for gbuffer pass to finish writing
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                            vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].dstAccessMask =
        // depth stencil
        vk::AccessFlagBits::eDepthStencilAttachmentRead |
        // gpass position_ms, normal_ms
        vk::AccessFlagBits::eShaderRead |
        // ao_ms, ao_res
        vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for ao_res to be written
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;  // read ao_res

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> AOPass::GetSubpasses() {
  ao_ms_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};
  ao_res_ref_ = {1, vk::ImageLayout::eColorAttachmentOptimal};
  depth_ref_ = {2, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

  vk::SubpassDescription subpass{};
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &ao_ms_ref_;
  subpass.pResolveAttachments = &ao_res_ref_;
  subpass.pDepthStencilAttachment = &depth_ref_;

  return {subpass};
}

std::vector<vk::ImageView> AOPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.ao_ms || !resrc.ao_res || !resrc.ds_ms) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {resrc.ao_ms->GetImageView(), resrc.ao_res->GetImageView(),
          resrc.ds_ms->GetImageView()};
}

/*
 * BlurPass
 */
void SingleColorPass::SetClearValues() { clear_values_.resize(1); }

std::vector<vk::AttachmentDescription> SingleColorPass::GetAttachments() const {
  std::vector<vk::AttachmentDescription> attachments(1);

  attachments[0].format = format_;
  attachments[0].samples = vk::SampleCountFlagBits::e1;
  attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  return attachments;
}

std::vector<vk::SubpassDependency> SingleColorPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    // wait for input image to be available
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].dstAccessMask =
        vk::AccessFlagBits::eShaderRead |           // sample input
        vk::AccessFlagBits::eColorAttachmentWrite;  // write output

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for blur pass to write output image
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[1].dstAccessMask =
        vk::AccessFlagBits::eShaderRead;  // read blurred image

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> SingleColorPass::GetSubpasses() {
  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};

  vk::SubpassDescription subpass{};
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref_;

  return {subpass};
}

std::vector<vk::ImageView> AOBlurHPass::GetAttachmentViews(
    int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.ao_temp)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.ao_temp->GetImageView()};
}

std::vector<vk::ImageView> AOBlurVPass::GetAttachmentViews(
    int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.ao_res)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.ao_res->GetImageView()};
}

std::vector<vk::ImageView> BrightPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.bright_color)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.bright_color->GetImageView()};
}

std::vector<vk::ImageView> BlurBrightPass::GetAttachmentViews(
    int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.bright_temp)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.bright_temp->GetImageView()};
}

std::vector<vk::ImageView> CocPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.coc_map) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {resrc.coc_map->GetImageView()};
}

std::vector<vk::ImageView> DofPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.present_color) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {resrc.present_color->GetImageView()};
}

/*
 * GlobLightPass
 */
void GlobLightPass::SetClearValues() {
  clear_values_.resize(2);

  // color_ms
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
}

std::vector<vk::AttachmentDescription> GlobLightPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(2);

  // color_ms
  attachments[0].format = props.color_format;
  attachments[0].samples = props.samples;
  attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // ds_ms
  attachments[1].format = props.ds_format;
  attachments[1].samples = props.samples;
  attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eLoad;
  attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eStore;
  attachments[1].initialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
  attachments[1].finalLayout =
      vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> GlobLightPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    // wait for gbuffer pass to finish writing and ao_res
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                            vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].dstAccessMask =
        // depth stencil
        vk::AccessFlagBits::eDepthStencilAttachmentRead |
        // read gbuff, ao_res
        vk::AccessFlagBits::eShaderRead |
        // write color_ms
        vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }
  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for color_ms to be written
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests;
    deps[1].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite |         // write color_ms
        vk::AccessFlagBits::eDepthStencilAttachmentWrite |  // write stencil
        vk::AccessFlagBits::eDepthStencilAttachmentRead;  // depth stencil tests

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> GlobLightPass::GetSubpasses() {
  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};
  ds_ref_ = {1, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

  vk::SubpassDescription subpass{};
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref_;
  subpass.pDepthStencilAttachment = &ds_ref_;

  return {subpass};
}

std::vector<vk::ImageView> GlobLightPass::GetAttachmentViews(
    int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];
  if (!resrc.color_ms || !resrc.ds_ms)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.color_ms->GetImageView(), resrc.ds_ms->GetImageView()};
}

/*
 * LocalLightPass
 */
void LocalLightPass::SetClearValues() {
  clear_values_.resize(2);

  //  ds_ms
  clear_values_[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
}

std::vector<vk::AttachmentDescription> LocalLightPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(2);

  // color_ms
  attachments[0].format = props.color_format;
  attachments[0].samples = props.samples;
  attachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
  attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // ds_ms
  attachments[1].format = props.ds_format;
  attachments[1].samples = props.samples;
  attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
  attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[1].initialLayout =
      vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal;
  attachments[1].finalLayout =
      vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> LocalLightPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(3);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    deps[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[0].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentRead |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[0].dstAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentRead |  // depth test
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;  // stencil write

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = 1;

    // wait for subpass 0 to write stencil
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[1].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].dstAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentRead |  // stencil test
        vk::AccessFlagBits::eColorAttachmentRead |         // blend
        vk::AccessFlagBits::eColorAttachmentWrite;         // write color_ms

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[2].srcSubpass = 1;
    deps[2].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for subpass 1 to finish writing color_ms
    deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[2].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[2].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead |  // next light reads
        vk::AccessFlagBits::eColorAttachmentWrite;  // next light writes

    deps[2].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> LocalLightPass::GetSubpasses() {
  std::vector<vk::SubpassDescription> subpasses(2);

  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};
  ds_ref_ = {1, vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal};

  subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpasses[0].colorAttachmentCount = 0;
  subpasses[0].pDepthStencilAttachment = &ds_ref_;

  subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpasses[1].colorAttachmentCount = 1;
  subpasses[1].pColorAttachments = &color_ref_;
  subpasses[1].pDepthStencilAttachment = &ds_ref_;

  return subpasses;
}

std::vector<vk::ImageView> LocalLightPass::GetAttachmentViews(
    int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];
  if (!resrc.color_ms || !resrc.ds_ms)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.color_ms->GetImageView(), resrc.ds_ms->GetImageView()};
}

/*
 * ABuffPass
 */
void ABuffPass::SetClearValues() { clear_values_.resize(2); }

std::vector<vk::AttachmentDescription> ABuffPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(2);

  // ds_ms
  attachments[0].format = props.ds_format;
  attachments[0].samples = props.samples;
  attachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[0].initialLayout =
      vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal;
  attachments[0].finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

  // color_res
  attachments[1].format = props.color_format;
  attachments[1].samples = vk::SampleCountFlagBits::e1;
  attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[1].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  attachments[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> ABuffPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(3);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    deps[0].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests |
                           vk::PipelineStageFlagBits::eTransfer |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentWrite |  // gbuff depth
        vk::AccessFlagBits::eTransferWrite |                // abuff clear
        vk::AccessFlagBits::eColorAttachmentWrite;          // color_res

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eFragmentShader;

    deps[0].dstAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentRead |  // depth testing
        vk::AccessFlagBits::eShaderWrite |  // write abuff storage
        vk::AccessFlagBits::eShaderRead;    // read abuff storage counter

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = 1;

    // wait for subpass 0 to write abuff storage
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[1].srcAccessMask = vk::AccessFlagBits::eShaderWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eFragmentShader;
    deps[1].dstAccessMask =
        // write color_res
        vk::AccessFlagBits::eColorAttachmentWrite |
        // read color_res for blending
        vk::AccessFlagBits::eColorAttachmentRead |
        // read abuff storage
        vk::AccessFlagBits::eShaderRead;

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[2].srcSubpass = 1;
    deps[2].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for subpass 1 to write present_color and bright_color
    deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[2].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[2].dstAccessMask = vk::AccessFlagBits::eShaderRead;

    deps[2].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> ABuffPass::GetSubpasses() {
  std::vector<vk::SubpassDescription> subpasses(2);

  {  // fill abuff (render transparent geometry, write to a-buffer)
    depth_ref_ = {0, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[0].colorAttachmentCount = 0;
    subpasses[0].pDepthStencilAttachment = &depth_ref_;
  }

  {  // resolve (fullscreen blend)
    color_ref_ = {1, vk::ImageLayout::eColorAttachmentOptimal};

    subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &color_ref_;
  }

  return subpasses;
}

std::vector<vk::ImageView> ABuffPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];
  if (!resrc.ds_ms || !resrc.color_res)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {resrc.ds_ms->GetImageView(), resrc.color_res->GetImageView()};
}

/*
 * WBoitPass
 */
void WBoitPass::SetClearValues() {
  clear_values_.resize(6);

  // acc_color_ms, acc_weight_ms
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
  clear_values_[1].color = std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f};
}

std::vector<vk::AttachmentDescription> WBoitPass::GetAttachments() const {
  auto& props = resrc_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(6);

  // acc_color_ms
  attachments[0].format = props.acc_color_format;
  attachments[0].samples = props.samples;
  attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[0].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // acc_weight_ms
  attachments[1].format = props.acc_weight_format;
  attachments[1].samples = props.samples;
  attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[1].initialLayout = vk::ImageLayout::eUndefined;
  attachments[1].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // acc_color_res
  attachments[2].format = props.acc_color_format;
  attachments[2].samples = vk::SampleCountFlagBits::e1;
  attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[2].initialLayout = vk::ImageLayout::eUndefined;
  attachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // acc_weight_res
  attachments[3].format = props.acc_weight_format;
  attachments[3].samples = vk::SampleCountFlagBits::e1;
  attachments[3].loadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[3].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[3].initialLayout = vk::ImageLayout::eUndefined;
  attachments[3].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // ds_ms
  attachments[4].format = props.ds_format;
  attachments[4].samples = props.samples;
  attachments[4].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[4].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[4].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[4].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[4].initialLayout =
      vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal;
  attachments[4].finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

  // color_res
  attachments[5].format = props.color_format;
  attachments[5].samples = vk::SampleCountFlagBits::e1;
  attachments[5].loadOp = vk::AttachmentLoadOp::eLoad;
  attachments[5].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[5].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  attachments[5].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> WBoitPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(3);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    // wait for previous depth/color to finish.
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                            vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].dstAccessMask =
        // depth testing
        vk::AccessFlagBits::eDepthStencilAttachmentRead |
        // acc_color_ms, acc_weight_ms writes
        vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = 1;

    // wait for subpass 0 to write and resolve accum
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].dstAccessMask =
        vk::AccessFlagBits::eInputAttachmentRead |  // read accum_res
        // read color_res for blending
        vk::AccessFlagBits::eColorAttachmentRead |
        // write color_res
        vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[2].srcSubpass = 1;
    deps[2].dstSubpass = VK_SUBPASS_EXTERNAL;

    // wait for subpass 1 to write color_res
    deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[2].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    // read color_res
    deps[2].dstAccessMask = vk::AccessFlagBits::eShaderRead;

    deps[2].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return deps;
}

std::vector<vk::SubpassDescription> WBoitPass::GetSubpasses() {
  std::vector<vk::SubpassDescription> subpasses(2);

  {  // acc
    // acc_color_ms and acc_weight_ms
    acc_refs_[0] = {0, vk::ImageLayout::eColorAttachmentOptimal};
    acc_refs_[1] = {1, vk::ImageLayout::eColorAttachmentOptimal};

    // acc_color_res and acc_weight_res
    resolve_refs_[0] = {2, vk::ImageLayout::eColorAttachmentOptimal};
    resolve_refs_[1] = {3, vk::ImageLayout::eColorAttachmentOptimal};

    depth_ref_ = {4, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[0].colorAttachmentCount = acc_refs_.size();
    subpasses[0].pColorAttachments = acc_refs_.data();
    subpasses[0].pResolveAttachments = resolve_refs_.data();
    subpasses[0].pDepthStencilAttachment = &depth_ref_;
  }

  {  // compose
    // acc_color_res, acc_weight_res
    input_refs_[0] = {2, vk::ImageLayout::eShaderReadOnlyOptimal};
    input_refs_[1] = {3, vk::ImageLayout::eShaderReadOnlyOptimal};

    // color_res
    color_ref_ = {5, vk::ImageLayout::eColorAttachmentOptimal};

    subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &color_ref_;
    subpasses[1].inputAttachmentCount = input_refs_.size();
    subpasses[1].pInputAttachments = input_refs_.data();
  }

  return subpasses;
}

std::vector<vk::ImageView> WBoitPass::GetAttachmentViews(int frame_idx) const {
  auto& resrc = resrc_.GetResrc()[frame_idx];

  if (!resrc.ds_ms || !resrc.acc_color_ms || !resrc.acc_weight_ms ||
      !resrc.acc_color_res || !resrc.acc_weight_res || !resrc.color_res) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {
      resrc.acc_color_ms->GetImageView(),  resrc.acc_weight_ms->GetImageView(),
      resrc.acc_color_res->GetImageView(), resrc.acc_weight_res->GetImageView(),
      resrc.ds_ms->GetImageView(),         resrc.color_res->GetImageView()};
}

/*
 * SwapPass
 */
void SwapPass::SetClearValues() {
  clear_values_.resize(1);

  // swapchain image
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
}

std::vector<vk::AttachmentDescription> SwapPass::GetAttachments() const {
  vk::AttachmentDescription color_attach{};

  // swapchain image
  color_attach.format = swapchain_.GetProps().format;
  color_attach.samples = vk::SampleCountFlagBits::e1;
  color_attach.loadOp = vk::AttachmentLoadOp::eClear;
  color_attach.storeOp = vk::AttachmentStoreOp::eStore;
  color_attach.initialLayout = vk::ImageLayout::eUndefined;
  color_attach.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  return {color_attach};
}

std::vector<vk::SubpassDependency> SwapPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  {
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;

    // wait for ABuffPass/WBoitPass to write present_color
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[0].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                           vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[0].dstAccessMask =
        vk::AccessFlagBits::eShaderRead |           // read present_color
        vk::AccessFlagBits::eColorAttachmentWrite;  // write swapchain

    deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  {
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = 1;

    // wait for swapchain image to be available
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    deps[1].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                            vk::AccessFlagBits::eColorAttachmentWrite;

    deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  // no need for a external dependency since this is the last pass
  // synchronization to present is handled by semaphores in the main loop
  // wait_stage = COLOR_ATTACHMENT_OUTPUT

  return deps;
}

std::vector<vk::SubpassDescription> SwapPass::GetSubpasses() {
  std::vector<vk::SubpassDescription> subpasses(2);

  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};
  subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &color_ref_;

  subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpasses[1].colorAttachmentCount = 1;
  subpasses[1].pColorAttachments = &color_ref_;

  return subpasses;
}

void SwapPass::CreateFramebuffers() {
  DestroyFramebuffers();

  auto swap_props = swapchain_.GetProps();
  const auto& image_views = swapchain_.GetViews();

  framebuffers_.resize(swap_props.image_count);

  for (size_t i = 0; i < image_views.size(); ++i) {
    vk::FramebufferCreateInfo framebuff_info{};
    framebuff_info.renderPass = render_pass_;
    framebuff_info.attachmentCount = 1;
    framebuff_info.pAttachments = &image_views[i];
    framebuff_info.width = swap_props.extent.width;
    framebuff_info.height = swap_props.extent.height;
    framebuff_info.layers = 1;

    framebuffers_[i] = ctx_.GetDevice().createFramebuffer(framebuff_info);
    ctx_.SetDbgName((uint64_t)(VkFramebuffer)framebuffers_[i],
                    vk::ObjectType::eFramebuffer,
                    GetDbgName() + "_framebuffer_" + std::to_string(i));
  }
}

}  // namespace npr_graphics
