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

/*
 * SwapPass
 */
void SwapPass::SetClearValues() {
  clear_values_.resize(1);
  clear_values_[0].color = std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f};
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
