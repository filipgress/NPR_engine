#include "render_pass.h"

namespace npr_graphics {
BasePass::~BasePass() {
  DestroyFramebuffers();
  if (render_pass_) c_.GetDevice().destroyRenderPass(render_pass_);
}

void BasePass::DestroyFramebuffers() {
  if (framebuffers_.empty()) return;

  for (const auto& framebuffer : framebuffers_)
    c_.GetDevice().destroyFramebuffer(framebuffer);
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

  render_pass_ = c_.GetDevice().createRenderPass(render_pass_info);
  c_.SetDbgName((uint64_t)(VkRenderPass)render_pass_,
                vk::ObjectType::eRenderPass, GetDbgName());
}

void RenderPass::CreateFramebuffers() {
  auto& resources = res_.GetResources();
  auto extent = res_.GetProps().extent;

  framebuffers_.reserve(resources.size());
  for (size_t i = 0; i < resources.size(); i++) {
    std::vector<vk::ImageView> attachments = GetAttachmentViews(i);

    vk::FramebufferCreateInfo framebuff_info{};
    framebuff_info.renderPass = render_pass_;
    framebuff_info.attachmentCount = attachments.size();
    framebuff_info.pAttachments = attachments.data();
    framebuff_info.width = extent.width;
    framebuff_info.height = extent.height;
    framebuff_info.layers = 1;

    framebuffers_.push_back(c_.GetDevice().createFramebuffer(framebuff_info));
  }
}

/*
 * GBuffPass
 */

void GBuffPass::SetClearValues() {
  clear_values_.resize(7);

  // albedo_ms, emissive_ms, position_ms, normal_ms, depth_stencil_ms,
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
  auto& props = res_.GetProps();
  std::vector<vk::AttachmentDescription> attachs(7);

  // albedo_ms
  attachs[0].format = props.albedo_format;
  attachs[0].samples = props.samples;
  attachs[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[0].initialLayout = vk::ImageLayout::eUndefined;
  attachs[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // emissive_ms
  attachs[1].format = props.emissive_format;
  attachs[1].samples = props.samples;
  attachs[1].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[1].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[1].initialLayout = vk::ImageLayout::eUndefined;
  attachs[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // position_ms
  attachs[2].format = props.position_format;
  attachs[2].samples = props.samples;
  attachs[2].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[2].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[2].initialLayout = vk::ImageLayout::eUndefined;
  attachs[2].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // normal_ms
  attachs[3].format = props.normal_format;
  attachs[3].samples = props.samples;
  attachs[3].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[3].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[3].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[3].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[3].initialLayout = vk::ImageLayout::eUndefined;
  attachs[3].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // depth_stencil_ms
  attachs[4].format = props.depth_stencil_format;
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
  attachs[5].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[5].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[5].initialLayout = vk::ImageLayout::eUndefined;
  attachs[5].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // coverage_res
  attachs[6].format = props.coverage_format;
  attachs[6].samples = vk::SampleCountFlagBits::e1;
  attachs[6].loadOp = vk::AttachmentLoadOp::eClear;
  attachs[6].storeOp = vk::AttachmentStoreOp::eStore;
  attachs[6].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachs[6].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachs[6].initialLayout = vk::ImageLayout::eUndefined;
  attachs[6].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  return attachs;
}

std::vector<vk::SubpassDependency> GBuffPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(3);

  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
  deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                         vk::PipelineStageFlagBits::eEarlyFragmentTests;
  deps[0].srcAccessMask = vk::AccessFlagBits::eNone;
  deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                          vk::AccessFlagBits::eDepthStencilAttachmentWrite;

  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = 1;
  deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  deps[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  deps[1].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  deps[2].srcSubpass = 1;
  deps[2].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                         vk::PipelineStageFlagBits::eEarlyFragmentTests;
  deps[2].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader |
                         vk::PipelineStageFlagBits::eEarlyFragmentTests;
  deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                          vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  deps[2].dstAccessMask = vk::AccessFlagBits::eShaderRead |
                          vk::AccessFlagBits::eDepthStencilAttachmentRead;

  return deps;
}

std::vector<vk::SubpassDescription> GBuffPass::GetSubpasses() {
  std::vector<vk::SubpassDescription> subpasses(2);

  {
    color_refs_[0] = {0, vk::ImageLayout::eColorAttachmentOptimal};  // albedo
    color_refs_[1] = {1, vk::ImageLayout::eColorAttachmentOptimal};  // emissive
    color_refs_[2] = {2, vk::ImageLayout::eColorAttachmentOptimal};  // position
    color_refs_[3] = {3, vk::ImageLayout::eColorAttachmentOptimal};  // normal
    color_refs_[4] = {5, vk::ImageLayout::eColorAttachmentOptimal};  // coverage
    depth_ref_ = {4, vk::ImageLayout::eDepthStencilAttachmentOptimal};

    subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[0].colorAttachmentCount = color_refs_.size();
    subpasses[0].pColorAttachments = color_refs_.data();
    subpasses[0].pDepthStencilAttachment = &depth_ref_;
  }

  {
    // coverage_res
    resolve_ref_ = {6, vk::ImageLayout::eColorAttachmentOptimal};

    subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &color_refs_[4];
    subpasses[1].pResolveAttachments = &resolve_ref_;
  }

  return subpasses;
}

std::vector<vk::ImageView> GBuffPass::GetAttachmentViews(int frame_idx) const {
  auto& res = res_.GetResources()[frame_idx];

  if (!res.albedo_metallic_ms || !res.emissive_roughness_ms ||
      !res.position_ms || !res.normal_ms || !res.depth_stencil_ms ||
      !res.coverage_ms || !res.coverage_res) {
    throw std::runtime_error("missing required resources for: " + GetDbgName());
  }

  return {res.albedo_metallic_ms->GetImageView(),
          res.emissive_roughness_ms->GetImageView(),
          res.position_ms->GetImageView(),
          res.normal_ms->GetImageView(),
          res.depth_stencil_ms->GetImageView(),
          res.coverage_ms->GetImageView(),
          res.coverage_res->GetImageView()};
}

/*
 * LoadPass
 */
void LoadPass::SetClearValues() {
  clear_values_.resize(1);
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
}

std::vector<vk::AttachmentDescription> LoadPass::GetAttachments() const {
  auto& props = res_.GetProps();
  std::vector<vk::AttachmentDescription> attachments(1);

  attachments[0].format = props.albedo_format;
  attachments[0].samples = vk::SampleCountFlagBits::e1;
  attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  return attachments;
}

std::vector<vk::SubpassDependency> LoadPass::GetDependencies() const {
  std::vector<vk::SubpassDependency> deps(2);

  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
  deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  deps[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
  deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
  deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;

  return deps;
}

std::vector<vk::SubpassDescription> LoadPass::GetSubpasses() {
  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};

  vk::SubpassDescription subpass{};
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref_;

  return {subpass};
}

std::vector<vk::ImageView> LoadPass::GetAttachmentViews(int frame_idx) const {
  auto& res = res_.GetResources()[frame_idx];
  if (!res.present_color)
    throw std::runtime_error("missing required resources for: " + GetDbgName());

  return {res.present_color->GetImageView()};
}

/*
 * SwapPass
 */
void SwapPass::SetClearValues() {
  clear_values_.resize(1);
  clear_values_[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
}

std::vector<vk::AttachmentDescription> SwapPass::GetAttachments() const {
  vk::AttachmentDescription color_attach{};
  color_attach.format = swapchain_.GetProps().format;
  color_attach.samples = vk::SampleCountFlagBits::e1;
  color_attach.loadOp = vk::AttachmentLoadOp::eClear;
  color_attach.storeOp = vk::AttachmentStoreOp::eStore;
  color_attach.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  color_attach.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  color_attach.initialLayout = vk::ImageLayout::eUndefined;
  color_attach.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  return {color_attach};
}

std::vector<vk::SubpassDependency> SwapPass::GetDependencies() const {
  return {};
}

std::vector<vk::SubpassDescription> SwapPass::GetSubpasses() {
  color_ref_ = {0, vk::ImageLayout::eColorAttachmentOptimal};

  vk::SubpassDescription subpass{};
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref_;

  return {subpass};
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

    framebuffers_[i] = c_.GetDevice().createFramebuffer(framebuff_info);
    c_.SetDbgName((uint64_t)(VkFramebuffer)framebuffers_[i],
                  vk::ObjectType::eFramebuffer,
                  GetDbgName() + "_framebuffer_" + std::to_string(i));
  }
}

}  // namespace npr_graphics
