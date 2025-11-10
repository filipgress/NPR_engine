#include "renderer.h"

namespace npr_graphics {
void Renderer::Render(const npr_scene::Camera& camera, npr_scene::Scene& scene,
                      bool is_loading, float dt) {
  if (!scene.IsValid() || !scene.IsInit()) return;
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
      return;  // skip frame
    case vk::Result::eErrorOutOfDateKHR:
      RenderTargetResize();
      return;
    default:
      throw std::runtime_error("failed to acquire swapchain image: " +
                               vk::to_string(res_acquire));
  }
  device.resetFences(in_flight);

  // record & submit commands
  // auto cmd_buff = (scene.IsValid() && scene.IsInit())
  //                     ? Record(image_idx, camera, scene, is_loading, dt)
  //                     : RecordFallback(image_idx, is_loading, dt);
  vk::CommandBuffer cmd_buff;
  if (scene.IsValid() && scene.IsInit()) {
    cmd_buff = Record(image_idx, camera, scene, is_loading, dt);
  }

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

vk::CommandBuffer Renderer::RecordFallback(uint image_idx, bool is_loading,
                                           float dt) {
  auto frame_idx = sync_.GetFrameIdx();
  auto cmd_buff = cmd_pool_.GetCmdBuff(frame_idx);

  cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  {  // load_pass
    auto res_extent = res_.GetProps().extent;
    cmd_buff.beginRenderPass(load_pass_.BeginInfo(frame_idx, res_extent),
                             vk::SubpassContents::eInline);
    cmd_buff.endRenderPass();
  }
  {  // swap_pass
    auto swap_extent = swapchain_.GetProps().extent;
    LoadPushConst load_data{};
    if (is_loading) {
      glm::ivec2 size = window_.GetSize();
      load_data.res = {size.x, size.y};
      load_data.t = {
          std::sin(dt * 2.4f) * 0.5f + 0.5f,
          std::sin(dt * 3.5f) * 0.5f + 0.5f,
          std::sin(dt * 2.8f) * 0.5f + 0.5f,
      };
      load_data.is_loading = true;
    }

    cmd_buff.beginRenderPass(swap_pass_.BeginInfo(image_idx, swap_extent),
                             vk::SubpassContents::eInline);

    vk::Rect2D scissor{{0, 0}, swap_extent};
    vk::Viewport viewport{0.0f,
                          0.0f,
                          static_cast<float>(swap_extent.width),
                          static_cast<float>(swap_extent.height),
                          0.0f,
                          1.0f};

    cmd_buff.setScissor(0, scissor);
    cmd_buff.setViewport(0, viewport);
    cmd_buff.setCullMode(vk::CullModeFlagBits::eNone);

    cmd_buff.pushConstants(swap_pipe_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0,
                           sizeof(load_data), &load_data);
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          swap_pipe_.GetPipeline());
    cmd_buff.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, swap_pipe_.GetLayout(), 0,
        desc_pool_.GetPresentSets().GetSet(frame_idx), {});

    cmd_buff.draw(3, 1, 0, 0);
    cmd_buff.endRenderPass();
  }

  cmd_buff.end();
  return cmd_buff;
}

vk::CommandBuffer Renderer::Record(uint image_idx,
                                   const npr_scene::Camera& camera,
                                   npr_scene::Scene& scene, bool is_loading,
                                   float dt) {
  auto frame_idx = sync_.GetFrameIdx();
  auto cmd_buff = cmd_pool_.GetCmdBuff(frame_idx);

  cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  {  // gbuff_pass
    auto res_extent = res_.GetProps().extent;
    cmd_buff.beginRenderPass(gbuff_pass_.BeginInfo(frame_idx, res_extent),
                             vk::SubpassContents::eInline);

    vk::Rect2D scissor{{0, 0}, res_extent};
    vk::Viewport viewport(0, 0, res_extent.width, res_extent.height, 0.0f,
                          1.0f);

    cmd_buff.setScissor(0, scissor);
    cmd_buff.setViewport(0, viewport);

    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          gbuff_pipe_.GetPipeline());

    res_.frame_resources_[frame_idx].camera_ubo->Write(camera.GetCameraUnif());
    cmd_buff.bindDescriptorSets(  // camera
        vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 0,
        desc_pool_.GetCameraSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // textures
        vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 1,
        scene.GetGpuResources().desc_pool.GetTextureSet().GetSet(0), {});

    scene.RecordOpaque(cmd_buff, gbuff_pipe_.GetLayout(), frame_idx, res_,
                       desc_pool_, camera.GetFrustum());

    cmd_buff.nextSubpass(vk::SubpassContents::eInline);
    cmd_buff.endRenderPass();
  }
  {  // swap_pass
    auto swap_extent = swapchain_.GetProps().extent;
    auto [viewport, scissor] =
        CalcViewportScissor(swap_extent, camera.GetAspect());

    LoadPushConst load_data{};
    if (is_loading) {
      glm::ivec2 size = window_.GetSize();
      load_data.res = {size.x, size.y};
      load_data.t = {
          std::sin(dt * 2.4f) * 0.5f + 0.5f,
          std::sin(dt * 3.5f) * 0.5f + 0.5f,
          std::sin(dt * 2.8f) * 0.5f + 0.5f,
      };
      load_data.is_loading = true;
    }

    cmd_buff.beginRenderPass(swap_pass_.BeginInfo(image_idx, swap_extent),
                             vk::SubpassContents::eInline);

    cmd_buff.setScissor(0, scissor);
    cmd_buff.setViewport(0, viewport);
    cmd_buff.setCullMode(vk::CullModeFlagBits::eNone);

    cmd_buff.pushConstants(swap_pipe_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0,
                           sizeof(load_data), &load_data);
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          swap_pipe_.GetPipeline());
    // cmd_buff.bindDescriptorSets(
    //     vk::PipelineBindPoint::eGraphics, swap_pipe_.GetLayout(), 0,
    //     desc_pool_.GetPresentSets().GetSet(frame_idx), {});
    cmd_buff.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, swap_pipe_.GetLayout(), 0,
        desc_pool_.GetGBuffSets().GetSet(frame_idx), {});

    cmd_buff.draw(3, 1, 0, 0);
    cmd_buff.endRenderPass();
  }

  cmd_buff.end();
  return cmd_buff;
}

std::pair<vk::Viewport, vk::Rect2D> Renderer::CalcViewportScissor(
    vk::Extent2D swap_extent, float camera_aspect) const {
  (void)camera_aspect;
  float win_aspect = static_cast<float>(swap_extent.width) / swap_extent.height;

  if (win_aspect > camera_aspect) {
    // Window is wider: letterbox (black bars on left/right)
    uint32_t width = swap_extent.height * camera_aspect;
    int32_t offset = (swap_extent.width - width) / 2;

    vk::Viewport viewport(offset, 0, width, swap_extent.height, 0.0f, 1.0f);
    vk::Rect2D scissor{{offset, 0}, {width, swap_extent.height}};

    return {viewport, scissor};
  } else {
    // Window is taller: pillarbox (black bars on top/bottom)
    uint32_t height = swap_extent.width / camera_aspect;
    int32_t offset = (swap_extent.height - height) / 2;

    vk::Viewport viewport(0, offset, swap_extent.width, height, 0.0f, 1.0f);
    vk::Rect2D scissor{{0, offset}, {swap_extent.width, height}};

    return {viewport, scissor};
  }
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
