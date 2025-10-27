#include "renderer.h"

namespace npr_graphics {
void Renderer::Render() {
  auto device = c_.GetDevice();

  // wait for in flight fence
  auto [in_flight, image_available] = sync_.GetFrameSyncObjs();
  vk::Result res_in_flight =
      device.waitForFences(in_flight, VK_TRUE, UINT64_MAX);

  if (res_in_flight != vk::Result::eSuccess)
    throw std::runtime_error("error while waiting for in_flight fence: " +
                             vk::to_string(res_in_flight));

  // acquire next image
  auto swapchain = swapchain_.GetSwapchain();
  auto [res_acquire, image_idx] = device.acquireNextImageKHR(
      swapchain, UINT64_MAX, image_available, nullptr);

  switch (res_acquire) {
    case vk::Result::eSuccess:
    case vk::Result::eSuboptimalKHR:
      break;
    case vk::Result::eTimeout:
    case vk::Result::eNotReady:
      return;  // Skip frame gracefully
    case vk::Result::eErrorOutOfDateKHR:
      RenderTargetResize();
      return;
    default:
      throw std::runtime_error("failed to acquire swapchain image: " +
                               vk::to_string(res_acquire));
  }

  device.resetFences(in_flight);

  // record & submit commands
  auto cmd_buff = Record(image_idx);
  auto render_finished = sync_.GetRenderFinished(image_idx);

  vk::SubmitInfo submit_info{};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &image_available;
  vk::PipelineStageFlags waitStages[]{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submit_info.pWaitDstStageMask = waitStages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buff;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_finished;

  c_.GetGraphicsQ().submit(submit_info, in_flight);

  // present to screen
  vk::PresentInfoKHR present_info{};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &image_idx;

  auto res_present = c_.GetPresentQ().presentKHR(present_info);
  if (res_present == vk::Result::eErrorOutOfDateKHR ||
      res_present == vk::Result::eSuboptimalKHR ||
      swapchain_.GetProps().dirty) {
    RenderTargetResize();
  } else if (res_present != vk::Result::eSuccess) {
    throw std::runtime_error("failed to present image: " +
                             vk::to_string(res_present));
  }

  sync_.Increment();
}

vk::CommandBuffer Renderer::Record(uint image_idx) {
  auto frame_idx = sync_.GetFrameIdx();

  auto cmd_buff = cmd_pool_.GetCmdBuff(frame_idx);
  cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  auto swap_extent = swapchain_.GetProps().extent;

  vk::Rect2D scissor{{0, 0}, swap_extent};
  vk::Viewport viewport(0, 0, swap_extent.width, swap_extent.height, 0.0f,
                        1.0f);

  {
    // auto res_extent = res_.GetProps().extent;
    // cmd_buff.beginRenderPass(gbuff_pass_.BeginInfo(frame_idx, res_extent),
    //                          vk::SubpassContents::eInline);
    //
    // ModelPushConst data;
    // cmd_buff.pushConstants(gbuff_pipe_.GetLayout(),
    //                        vk::ShaderStageFlagBits::eVertex, 0, sizeof(data),
    //                        &data);
    //
    // auto offset =
    //     res_.GetResources()[frame_idx].material_unif->GetElemOffset(0);
    //
    // cmd_buff.setScissor(0, scissor);
    // cmd_buff.setViewport(0, viewport);
    // cmd_buff.setCullMode(vk::CullModeFlagBits::eNone);
    //
    // cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
    //                       gbuff_pipe_.GetPipeline());
    // cmd_buff.bindDescriptorSets(
    //     vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 0,
    //     desc_pool_.GetCameraSets().GetSet(frame_idx), {});
    // cmd_buff.bindDescriptorSets(
    //     vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 1,
    //     desc_pool_.GetMaterialSets().GetSet(frame_idx), offset);
    // cmd_buff.bindDescriptorSets(
    //     vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 2,
    //     res_.scene_.GetGpuResources().desc_pool->GetTextureSet().GetSet(0),
    //     {});
    //
    // auto& ibos = res_.scene_.GetGpuResources().ibos;
    // vk::Buffer vbos[] = {res_.scene_.GetGpuResources().vbos[0].GetBuffer()};
    // vk::DeviceSize offsets[] = {0};
    //
    // cmd_buff.bindVertexBuffers(0, 1, vbos, offsets);
    // cmd_buff.bindIndexBuffer(ibos[0].GetBuffer(), 0, vk::IndexType::eUint32);
    // cmd_buff.drawIndexed(ibos[0].GetCount(), 1, 0, 0, 0);
    //
    // cmd_buff.nextSubpass(vk::SubpassContents::eInline);
    // cmd_buff.endRenderPass();
  }
  {
    cmd_buff.beginRenderPass(swap_pass_.BeginInfo(image_idx, swap_extent),
                             vk::SubpassContents::eInline);

    cmd_buff.setScissor(0, scissor);
    cmd_buff.setViewport(0, viewport);
    cmd_buff.setCullMode(vk::CullModeFlagBits::eNone);

    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          swap_pipe_.GetPipeline());
    cmd_buff.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, swap_pipe_.GetLayout(), 0,
        desc_pool_.GetGBuffSets().GetSet(frame_idx), {});

    cmd_buff.draw(3, 1, 0, 0);

    cmd_buff.endRenderPass();
  }

  cmd_buff.end();
  return cmd_buff;
}

void Renderer::RenderTargetResize() {
  c_.GetDevice().waitIdle();

  swapchain_.Recreate(window_.GetSize());
  swap_pass_.CreateFramebuffers();
}

void Renderer::RecompileShaders() {
  if (!kEnableShaderReload) return;

  INFO("Reloading shaders");

  for (auto& vert_shader : vert_shaders_) vert_shader.ReloadAsync();
  for (auto& frag_shader : frag_shaders_) frag_shader.ReloadAsync();
}

void Renderer::SwapShaders() {
  if (!kEnableShaderReload) return;

  bool is_dirty{false};
  for (auto& vert_shader : vert_shaders_) {
    if (vert_shader.IsDirty()) {
      is_dirty = true;
      vert_shader.CreateShaderModule();
    }
  }
  for (auto& frag_shader : frag_shaders_) {
    if (frag_shader.IsDirty()) {
      is_dirty = true;
      frag_shader.CreateShaderModule();
    }
  }

  if (!is_dirty) return;
  c_.GetDevice().waitIdle();

  if (!swap_pipe_.IsUpToDate()) swap_pipe_.Recreate();
  if (!gbuff_pipe_.IsUpToDate()) gbuff_pipe_.Recreate();
}

}  // namespace npr_graphics
