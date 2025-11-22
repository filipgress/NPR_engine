#include "renderer.h"

using namespace npr_scene;

namespace npr_graphics {

uint Renderer::mat_at{0};
uint Renderer::inst_at{0};

void Renderer::Render(const Camera& camera, Scene& scene, bool is_loading,
                      float dt) {
  if (!scene.IsValid() || !scene.IsInit()) return;
  auto device = ctx_.GetDevice();

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
  vk::CommandBuffer cmd_buff;
  if (scene.IsValid() && scene.IsInit()) {
    gui_manager_.NewFrame();
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

  ctx_.GetGraphicsQ().submit(submit_info, in_flight);

  // present to screen
  vk::PresentInfoKHR present_info{};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &image_idx;

  auto res_present = ctx_.GetPresentQ().presentKHR(present_info);
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

vk::CommandBuffer Renderer::Record(uint image_idx, const Camera& camera,
                                   Scene& scene, bool is_loading, float dt) {
  auto frame_idx = sync_.GetFrameIdx();
  auto cmd_buff = cmd_pool_.GetCmdBuff(frame_idx);

  const auto& frame_resrc = resrc_.GetResrc()[frame_idx];
  auto resrc_extent = resrc_.GetProps().extent;

  frame_resrc.camera_ubo->Write(camera.GetCameraUnif());

  vk::Rect2D scissor{{0, 0}, resrc_extent};
  vk::Viewport viewport(0, 0, resrc_extent.width, resrc_extent.height, 0.0f,
                        1.0f);

  cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  cmd_buff.setScissor(0, scissor);
  cmd_buff.setViewport(0, viewport);

  RecordGBufferPass(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera,
                    scene);
  RecordAOPass(cmd_buff, frame_idx, resrc_extent);
  RecordLightPass(cmd_buff, frame_idx, resrc_extent);
  // RecordABufferPass(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera,
  //                   scene);
  // RecordWBoitPass(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera,
  //                 scene);
  RecordSwapPass(cmd_buff, image_idx, frame_idx, camera.GetAspect(), is_loading,
                 dt);

  cmd_buff.end();
  return cmd_buff;
}

void Renderer::RecordGBufferPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                                 const npr_graphics::FrameResources& frame_res,
                                 const vk::Extent2D& resrc_extent,
                                 const Camera& camera, Scene& scene) {
  cmd_buff.beginRenderPass(gbuff_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        gbuff_pipe_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // camera
      vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 0,
      desc_pool_.GetCameraSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // textures
      vk::PipelineBindPoint::eGraphics, gbuff_pipe_.GetLayout(), 1,
      scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

  RecordOpaque(cmd_buff, frame_idx, frame_res, scene, camera.GetFrustum());

  cmd_buff.nextSubpass(vk::SubpassContents::eInline);  // resolve coverage_ms
  cmd_buff.endRenderPass();
}

void Renderer::RecordOpaque(vk::CommandBuffer cmd_buff, uint frame_idx,
                            const npr_graphics::FrameResources& frame_resrc,
                            const npr_scene::Scene& scene,
                            const npr_scene::Frustum& frustum) const {
  const auto& instances = scene.GetInstances();
  const auto& scene_resrc = scene.GetResrc();
  const auto& material_set = desc_pool_.GetMaterialSets().GetSet(frame_idx);

  for (const auto& [key, instances] : instances) {
    const auto& mat = key.mat;
    if (instances.empty() || (!key.mat.is_mask && !mat.is_opaque)) continue;

    std::vector<InstanceData> visible;
    visible.reserve(instances.size());

    for (const auto& [tf, bb] : instances) {
      if (!frustum.IsVisible(bb)) continue;
      visible.push_back({.model = tf.glob_mat,
                         .normal = glm::transpose(glm::inverse(tf.glob_mat))});
    }

    if (visible.empty()) continue;
    cmd_buff.setCullMode(mat.double_sided ? vk::CullModeFlagBits::eNone
                                          : vk::CullModeFlagBits::eBack);

    uint32_t flags = MaterialFlags::kNone;
    if (mat.double_sided) flags |= MaterialFlags::kDoubleSided;
    if (mat.is_opaque) flags |= MaterialFlags::kOpaque;
    if (mat.is_mask) flags |= MaterialFlags::kMask;

    auto& material_ubo = *frame_resrc.material_ubo;
    material_ubo.Write(
        mat_at,
        MaterialUnif{{mat.color_map_idx, mat.normal_map_idx,
                      mat.metallic_roughness_map_idx, mat.emissive_map_idx},
                     mat.color_factor,
                     glm::vec4(mat.emissive_factor, 0.0f),
                     mat.metallic_factor,
                     mat.roughness_factor,
                     mat.alpha_cutoff,
                     flags});

    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                gbuff_pipe_.GetLayout(), 2, material_set,
                                material_ubo.GetElemOffset(mat_at));

    if (inst_at + visible.size() >= MAX_INSTANCES) inst_at = 0;
    frame_resrc.instance_buff->Write(visible, inst_at);

    std::array<vk::DeviceSize, 2> offsets = {0, inst_at * sizeof(InstanceData)};
    std::array<vk::Buffer, 2> buffers = {
        scene_resrc.vbos[key.mesh.vbo_idx].GetBuffer(),
        frame_resrc.instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      vk::Buffer ibo = scene_resrc.ibos[key.mesh.ibo_idx].GetBuffer();
      cmd_buff.bindIndexBuffer(ibo, 0, vk::IndexType::eUint32);
      uint32_t index_count = scene_resrc.ibos[key.mesh.ibo_idx].GetCount();
      cmd_buff.drawIndexed(index_count, visible.size(), 0, 0, 0);
    } else {
      uint32_t vertex_count = scene_resrc.vbos[key.mesh.vbo_idx].GetCount();
      cmd_buff.draw(vertex_count, visible.size(), 0, 0);
    }

    mat_at = (mat_at + 1) % MAX_MATERIALS;
    inst_at = (inst_at + visible.size()) % MAX_INSTANCES;
  }
}

void Renderer::RecordAOPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                            const vk::Extent2D& resrc_extent) {
  cmd_buff.beginRenderPass(ao_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        ao_gen_pipe_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // camera
      vk::PipelineBindPoint::eGraphics, ao_gen_pipe_.GetLayout(), 0,
      desc_pool_.GetCameraSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // gbuff
      vk::PipelineBindPoint::eGraphics, ao_gen_pipe_.GetLayout(), 1,
      desc_pool_.GetGBuffSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // kernel + noise texture
      vk::PipelineBindPoint::eGraphics, ao_gen_pipe_.GetLayout(), 2,
      desc_pool_.GetAOSet().GetSet(0), {});

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // horizontal blur pass
  cmd_buff.beginRenderPass(ao_blur_h_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        ao_blur_pipe_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // ao_res
      vk::PipelineBindPoint::eGraphics, ao_blur_pipe_.GetLayout(), 0,
      desc_pool_.GetAOResSets().GetSet(frame_idx), {});

  auto& blur_pc = resrc_.GetSSAOBlurPC();
  blur_pc.flags.x = 0;  // horizontal
  cmd_buff.pushConstants(ao_blur_pipe_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(BlurPushConst), &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // vertical blur pass
  cmd_buff.beginRenderPass(ao_blur_v_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        ao_blur_pipe_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // ao_temp input
      vk::PipelineBindPoint::eGraphics, ao_blur_pipe_.GetLayout(), 0,
      desc_pool_.GetAOTempSets().GetSet(frame_idx), {});

  blur_pc.flags.x = 1;  // vertical
  cmd_buff.pushConstants(ao_blur_pipe_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(BlurPushConst), &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();
}

void Renderer::RecordLightPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                               const vk::Extent2D& resrc_extent) {
  cmd_buff.beginRenderPass(light_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);
  cmd_buff.endRenderPass();
}

void Renderer::RecordABufferPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                                 const npr_graphics::FrameResources& frame_res,
                                 const vk::Extent2D& resrc_extent,
                                 const npr_scene::Camera& camera,
                                 Scene& scene) {
  // clear abuff before fill pass
  const uint32_t null_ptr = 0xFFFFFFFF;

  cmd_buff.fillBuffer(frame_res.abuff_heads->GetBuffer(), 0, VK_WHOLE_SIZE,
                      null_ptr);
  cmd_buff.fillBuffer(frame_res.abuff_counter->GetBuffer(), 0, VK_WHOLE_SIZE,
                      0);

  cmd_buff.beginRenderPass(abuff_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  {  // fill
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          abuff_fill_pipe_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // camera
        vk::PipelineBindPoint::eGraphics, abuff_fill_pipe_.GetLayout(), 0,
        desc_pool_.GetCameraSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // textures
        vk::PipelineBindPoint::eGraphics, abuff_fill_pipe_.GetLayout(), 1,
        scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

    cmd_buff.bindDescriptorSets(  // abuff
        vk::PipelineBindPoint::eGraphics, abuff_fill_pipe_.GetLayout(), 2,
        desc_pool_.GetABufferSets().GetSet(frame_idx), {});

    ABuffFillPushConst push_const{
        resrc_extent.width,
        resrc_extent.width * resrc_extent.height * ABUFF_INIT_SIZE,
        {}};

    cmd_buff.pushConstants(abuff_fill_pipe_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0,
                           sizeof(push_const), &push_const);

    RecordTrans(cmd_buff, frame_idx, frame_res, scene, camera.GetFrustum(),
                abuff_fill_pipe_.GetLayout(), 3);
  }

  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  {  // resolve
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          abuff_resolve_pipe_.GetPipeline());

    cmd_buff.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, abuff_resolve_pipe_.GetLayout(), 0,
        desc_pool_.GetABufferSets().GetSet(frame_idx), {});

    cmd_buff.pushConstants(abuff_resolve_pipe_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0,
                           sizeof(resrc_extent.width), &resrc_extent.width);

    cmd_buff.draw(3, 1, 0, 0);
  }

  cmd_buff.endRenderPass();
}

void Renderer::RecordWBoitPass(vk::CommandBuffer cmd_buff, uint frame_idx,
                               const npr_graphics::FrameResources& frame_res,
                               const vk::Extent2D& resrc_extent,
                               const npr_scene::Camera& camera, Scene& scene) {
  cmd_buff.beginRenderPass(wboit_pass_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  {  // acc
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          wboit_acc_pipe_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // camera
        vk::PipelineBindPoint::eGraphics, wboit_acc_pipe_.GetLayout(), 0,
        desc_pool_.GetCameraSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // textures
        vk::PipelineBindPoint::eGraphics, wboit_acc_pipe_.GetLayout(), 1,
        scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

    RecordTrans(cmd_buff, frame_idx, frame_res, scene, camera.GetFrustum(),
                wboit_acc_pipe_.GetLayout(), 2);
  }

  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  {  // compose
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          wboit_compose_pipe_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // input attachments
        vk::PipelineBindPoint::eGraphics, wboit_compose_pipe_.GetLayout(), 0,
        desc_pool_.GetWBoitInputSets().GetSet(frame_idx), {});
    cmd_buff.draw(3, 1, 0, 0);
  }

  cmd_buff.endRenderPass();
}

void Renderer::RecordTrans(vk::CommandBuffer cmd_buff, uint frame_idx,
                           const npr_graphics::FrameResources& frame_resrc,
                           const npr_scene::Scene& scene,
                           const npr_scene::Frustum& frustum,
                           vk::PipelineLayout layout,
                           const uint set_idx) const {
  const auto& instances = scene.GetInstances();
  const auto& scene_resrc = scene.GetResrc();
  const auto& material_set = desc_pool_.GetMaterialSets().GetSet(frame_idx);

  for (const auto& [key, instances] : instances) {
    const auto& mat = key.mat;
    if (instances.empty() || (key.mat.is_mask || mat.is_opaque)) continue;

    std::vector<InstanceData> visible;
    visible.reserve(instances.size());

    for (const auto& [tf, bb] : instances) {
      if (!frustum.IsVisible(bb)) continue;
      visible.push_back({.model = tf.glob_mat,
                         .normal = glm::transpose(glm::inverse(tf.glob_mat))});
    }

    if (visible.empty()) continue;

    uint32_t flags = MaterialFlags::kNone;
    if (mat.double_sided) flags |= MaterialFlags::kDoubleSided;
    if (mat.is_opaque) flags |= MaterialFlags::kOpaque;
    if (mat.is_mask) flags |= MaterialFlags::kMask;

    auto& material_ubo = *frame_resrc.material_ubo;
    material_ubo.Write(
        mat_at,
        MaterialUnif{{mat.color_map_idx, mat.normal_map_idx,
                      mat.metallic_roughness_map_idx, mat.emissive_map_idx},
                     mat.color_factor,
                     glm::vec4(mat.emissive_factor, 0.0f),
                     mat.metallic_factor,
                     mat.roughness_factor,
                     mat.alpha_cutoff,
                     flags});

    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                                set_idx, material_set,
                                material_ubo.GetElemOffset(mat_at));

    if (inst_at + visible.size() >= MAX_INSTANCES) inst_at = 0;
    frame_resrc.instance_buff->Write(visible, inst_at);

    std::array<vk::DeviceSize, 2> offsets = {0, inst_at * sizeof(InstanceData)};
    std::array<vk::Buffer, 2> buffers = {
        scene_resrc.vbos[key.mesh.vbo_idx].GetBuffer(),
        frame_resrc.instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      vk::Buffer ibo = scene_resrc.ibos[key.mesh.ibo_idx].GetBuffer();
      cmd_buff.bindIndexBuffer(ibo, 0, vk::IndexType::eUint32);
      uint32_t index_count = scene_resrc.ibos[key.mesh.ibo_idx].GetCount();
      cmd_buff.drawIndexed(index_count, visible.size(), 0, 0, 0);
    } else {
      uint32_t vertex_count = scene_resrc.vbos[key.mesh.vbo_idx].GetCount();
      cmd_buff.draw(vertex_count, visible.size(), 0, 0);
    }

    mat_at = (mat_at + 1) % MAX_MATERIALS;
    inst_at = (inst_at + visible.size()) % MAX_INSTANCES;
  }
}

void Renderer::RecordSwapPass(vk::CommandBuffer cmd_buff, uint image_idx,
                              uint frame_idx, float camera_aspect,
                              bool is_loading, float dt) {
  auto swap_extent = swapchain_.GetProps().extent;
  auto [viewport, scissor] = CalcViewportScissor(swap_extent, camera_aspect);

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

  cmd_buff.pushConstants(swap_pipe_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(load_data), &load_data);
  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        swap_pipe_.GetPipeline());
  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              swap_pipe_.GetLayout(), 0,
                              desc_pool_.GetAOResSets().GetSet(frame_idx), {});
  // cmd_buff.bindDescriptorSets(
  //     vk::PipelineBindPoint::eGraphics, swap_pipe_.GetLayout(), 0,
  //     desc_pool_.GetPresentSets().GetSet(frame_idx), {});
  // cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
  //                             swap_pipe_.GetLayout(), 0,
  //                             desc_pool_.GetGBuffSets().GetSet(frame_idx),
  //                             {});

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  gui_manager_.RecordUI(cmd_buff);

  cmd_buff.endRenderPass();
}

std::pair<vk::Viewport, vk::Rect2D> Renderer::CalcViewportScissor(
    vk::Extent2D swap_extent, float camera_aspect) const {
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
  ctx_.GetDevice().waitIdle();

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
  ctx_.GetDevice().waitIdle();

  if (!gbuff_pipe_.IsUpToDate()) gbuff_pipe_.Recreate();
  if (!ao_gen_pipe_.IsUpToDate()) ao_gen_pipe_.Recreate();
  if (!ao_blur_pipe_.IsUpToDate()) ao_blur_pipe_.Recreate();
  if (!abuff_fill_pipe_.IsUpToDate()) abuff_fill_pipe_.Recreate();
  if (!abuff_resolve_pipe_.IsUpToDate()) abuff_resolve_pipe_.Recreate();
  if (!wboit_acc_pipe_.IsUpToDate()) wboit_acc_pipe_.Recreate();
  if (!wboit_compose_pipe_.IsUpToDate()) wboit_compose_pipe_.Recreate();
  if (!swap_pipe_.IsUpToDate()) swap_pipe_.Recreate();
}

}  // namespace npr_graphics
