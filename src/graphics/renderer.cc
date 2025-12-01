#include "renderer.h"

using namespace npr_scene;

namespace npr_graphics {

uint Renderer::mat_at{0};
uint Renderer::inst_at{0};

void Renderer::Update() {
  bool shaders_dirty = shaders_.IsDirty();
  bool dirty = shaders_dirty || settings_.dirty_target_size ||
               settings_.dirty_abuff_size;
  if (!dirty) return;

  WaitIdle();

  if (shaders_dirty) pipelines_.RebuildPipes();
  if (settings_.dirty_target_size) {
    resrc_.frame_props_.extent = settings_.target_size;

    resrc_.CreateImages();
    desc_pool_.UpdateDescriptors(resrc_);
    passes_.RecreateFramebuffers();

    settings_.dirty_target_size = false;
    settings_.dirty_abuff_size = true;
  }

  if (settings_.dirty_abuff_size) {
    resrc_.frame_props_.abuff_avg_nodes = settings_.abuff_avg_nodes;

    resrc_.CreateABuffers();
    desc_pool_.GetABufferSets().Update(resrc_);
    settings_.dirty_abuff_size = false;
  }
}

void Renderer::SwapTargetResize() {
  WaitIdle();

  swapchain_.Recreate(window_.GetSize());
  passes_.swap_.CreateFramebuffers();
}

void Renderer::Render(npr_core::FrameTimer& timer, Camera& camera, Scene& scene,
                      bool is_loading) {
  if (!scene.IsLoaded() || !scene.IsInit()) return;
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
      SwapTargetResize();
      return;
    default:
      throw std::runtime_error("failed to acquire swapchain image: " +
                               vk::to_string(res_acquire));
  }
  device.resetFences(in_flight);

  // record & submit commands
  vk::CommandBuffer cmd_buff;
  gui_.NewFrame(settings_, timer, camera, scene);
  cmd_buff = Record(image_idx, camera, scene, is_loading, timer.GetElapsed());

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
    SwapTargetResize();
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
  auto resrc_extent = resrc_.frame_props_.extent;

  const auto& cam_ubo = camera.GetCameraUnif();
  frame_resrc.camera_ubo->Write(cam_ubo);

  vk::Rect2D scissor{{0, 0}, resrc_extent};
  vk::Viewport viewport(0, 0, resrc_extent.width, resrc_extent.height, 0.0f,
                        1.0f);

  cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  cmd_buff.setScissor(0, scissor);
  cmd_buff.setViewport(0, viewport);

  RecordGBuff(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera, scene);
  if (settings_.enable_ssao) {
    RecordAO(cmd_buff, frame_idx, resrc_extent);
  } else {
    frame_resrc.ao_res->Transition(cmd_buff, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eShaderReadOnlyOptimal, 0,
                                   1);
  }

  RecordGlobLight(cmd_buff, frame_idx, frame_resrc, resrc_extent, cam_ubo,
                  scene);
  RecordLocalLight(cmd_buff, frame_idx, frame_resrc, resrc_extent, cam_ubo,
                   camera, scene);

  if (settings_.trans_mode == TransparencyMode::kABuff)
    RecordABuff(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera, scene);
  else if (settings_.trans_mode == TransparencyMode::kWBoit)
    RecordWBoit(cmd_buff, frame_idx, frame_resrc, resrc_extent, camera, scene);

  if (settings_.enable_bloom) RecordBloom(cmd_buff, frame_idx, resrc_extent);
  if (settings_.enable_dof)
    RecordDoF(cmd_buff, frame_idx, resrc_extent, camera);

  RecordSwap(cmd_buff, image_idx, frame_idx, camera, is_loading, dt);

  cmd_buff.end();
  return cmd_buff;
}

void Renderer::RecordGBuff(vk::CommandBuffer cmd_buff, const uint frame_idx,
                           const npr_graphics::FrameResources& frame_resrc,
                           const vk::Extent2D& resrc_extent,
                           const Camera& camera, Scene& scene) {
  cmd_buff.beginRenderPass(passes_.gbuff_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.gbuff_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // camera
      vk::PipelineBindPoint::eGraphics, pipelines_.gbuff_.GetLayout(), 0,
      desc_pool_.GetCameraSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // textures
      vk::PipelineBindPoint::eGraphics, pipelines_.gbuff_.GetLayout(), 1,
      scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

  RecordOpaque(cmd_buff, frame_idx, frame_resrc, scene, camera.GetFrustum());
  cmd_buff.endRenderPass();
}

void Renderer::RecordOpaque(vk::CommandBuffer cmd_buff, const uint frame_idx,
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

    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,  // material
                                pipelines_.gbuff_.GetLayout(), 2, material_set,
                                material_ubo.GetElemOffset(mat_at));

    if (inst_at + visible.size() >= kMaxInstances) inst_at = 0;
    frame_resrc.instance_buff->Write(visible, inst_at);

    std::array<vk::DeviceSize, 2> offsets = {0, inst_at * sizeof(InstanceData)};
    std::array<vk::Buffer, 2> buffers = {
        scene_resrc.vbos[key.mesh.vbo_idx].GetBuffer(),
        frame_resrc.instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      cmd_buff.bindIndexBuffer(scene_resrc.ibos[key.mesh.ibo_idx].GetBuffer(),
                               0, vk::IndexType::eUint32);
      cmd_buff.drawIndexed(scene_resrc.ibos[key.mesh.ibo_idx].GetCount(),
                           visible.size(), 0, 0, 0);
    } else {
      cmd_buff.draw(scene_resrc.vbos[key.mesh.vbo_idx].GetCount(),
                    visible.size(), 0, 0);
    }

    mat_at = (mat_at + 1) % kMaxMaterials;
    inst_at = (inst_at + visible.size()) % kMaxInstances;
  }
}

void Renderer::RecordAO(vk::CommandBuffer cmd_buff, const uint frame_idx,
                        const vk::Extent2D& resrc_extent) {
  cmd_buff.beginRenderPass(passes_.ao_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.ao_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // camera
      vk::PipelineBindPoint::eGraphics, pipelines_.ao_.GetLayout(), 0,
      desc_pool_.GetCameraSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // gbuff
      vk::PipelineBindPoint::eGraphics, pipelines_.ao_.GetLayout(), 1,
      desc_pool_.GetGBuffSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(  // kernel + noise texture
      vk::PipelineBindPoint::eGraphics, pipelines_.ao_.GetLayout(), 2,
      desc_pool_.GetAOSet().GetSet(0), {});

  AOPushConst ao_pc = {settings_.ssao_radius, settings_.ssao_bias};
  cmd_buff.pushConstants(pipelines_.ao_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(ao_pc),
                         &ao_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // horizontal blur pass
  cmd_buff.beginRenderPass(
      passes_.ao_blur_h_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.blur_ao_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // ao_res
      vk::PipelineBindPoint::eGraphics, pipelines_.blur_ao_.GetLayout(), 0,
      desc_pool_.GetAOResSets().GetSet(frame_idx), {});

  auto& blur_pc = resrc_.GetSSAOBlurPC();
  blur_pc.flags.x = 0;  // horizontal
  cmd_buff.pushConstants(pipelines_.blur_ao_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(blur_pc),
                         &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // vertical blur pass
  cmd_buff.beginRenderPass(
      passes_.ao_blur_v_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.blur_ao_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // ao_temp
      vk::PipelineBindPoint::eGraphics, pipelines_.blur_ao_.GetLayout(), 0,
      desc_pool_.GetAOTempSets().GetSet(frame_idx), {});

  blur_pc.flags.x = 1;  // vertical
  cmd_buff.pushConstants(pipelines_.blur_ao_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(blur_pc),
                         &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();
}

void Renderer::RecordGlobLight(vk::CommandBuffer cmd_buff, const uint frame_idx,
                               const npr_graphics::FrameResources& frame_resrc,
                               const vk::Extent2D& resrc_extent,
                               const CameraUnif& cam_ubo, Scene& scene) {
  DirLightUnif dir_lights{};
  dir_lights.ambient =
      glm::vec4(settings_.ambient_color * settings_.ambient_intensity,
                settings_.ambient_intensity);

  dir_lights.rim = glm::vec4{settings_.rim_color * settings_.rim_intensity,
                             settings_.rim_intensity};

  dir_lights.rim_power = settings_.rim_power;
  dir_lights.inv_rim = settings_.inv_rim ? 1 : 0;

  uint32_t i{0};
  scene.GetDirLightQuery().each([&](const DirLightTag&, const TransformComp& tf,
                                    const LightComp& light) {
    if (i >= kMaxDirLights) return;

    glm::mat3 world_rot = glm::mat3(tf.glob_mat);
    world_rot[0] = glm::normalize(world_rot[0]);
    world_rot[1] = glm::normalize(world_rot[1]);
    world_rot[2] = glm::normalize(world_rot[2]);

    glm::vec3 world_dir = world_rot * glm::vec3(0, 0, -1);
    glm::vec3 view_dir = glm::normalize(glm::mat3(cam_ubo.view) * world_dir);

    dir_lights.dir_lights[i].dir = glm::vec4(-view_dir, 0.0f);
    dir_lights.dir_lights[i].color =
        glm::vec4(light.color * light.intensity, 1.0f);

    i++;
  });

  dir_lights.count = i;
  dir_lights.use_ssao = settings_.enable_ssao ? 1 : 0;

  frame_resrc.dir_light_ubo->Write(dir_lights);

  cmd_buff.beginRenderPass(
      passes_.glob_light_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.glob_light_.GetPipeline());

  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,  // ao_res
                              pipelines_.glob_light_.GetLayout(), 0,
                              desc_pool_.GetAOResSets().GetSet(frame_idx), {});

  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,  // gbuff
                              pipelines_.glob_light_.GetLayout(), 1,
                              desc_pool_.GetGBuffSets().GetSet(frame_idx), {});

  LightPushConst light_pc{
      {}, settings_.diff_int, settings_.spec_int, settings_.is_pbr};

  cmd_buff.pushConstants(pipelines_.glob_light_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(light_pc), &light_pc);

  cmd_buff.bindDescriptorSets(  // dir lights
      vk::PipelineBindPoint::eGraphics, pipelines_.glob_light_.GetLayout(), 2,
      desc_pool_.GetDirLightSets().GetSet(frame_idx), {});

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();
}

void Renderer::RecordLocalLight(vk::CommandBuffer cmd_buff,
                                const uint frame_idx,
                                const npr_graphics::FrameResources& frame_resrc,
                                const vk::Extent2D& resrc_extent,
                                const CameraUnif& cam_ubo,
                                const npr_scene::Camera& camera,
                                npr_scene::Scene& scene) {
  RecordPointLights(cmd_buff, frame_idx, frame_resrc, resrc_extent, cam_ubo,
                    camera, scene);
  RecordSpotLights(cmd_buff, frame_idx, frame_resrc, resrc_extent, cam_ubo,
                   camera, scene);

  // resolve color_ms to color_res
  auto& color_ms = *frame_resrc.color_ms;
  auto& color_res = *frame_resrc.color_res;

  color_ms.Resolve(cmd_buff, color_res,
                   vk::ImageLayout::eColorAttachmentOptimal,
                   vk::ImageLayout::eUndefined);
  color_res.Transition(cmd_buff, vk::ImageLayout::eTransferDstOptimal,
                       vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
}

void Renderer::RecordPointLights(
    vk::CommandBuffer cmd_buff, const uint frame_idx,
    const npr_graphics::FrameResources& frame_resrc,
    const vk::Extent2D& resrc_extent, const CameraUnif& cam_ubo,
    const npr_scene::Camera& camera, npr_scene::Scene& scene) {
  const auto& frustum = camera.GetFrustum();
  const auto& sphere_mesh = resrc_.GetSphereMesh();
  auto& point_ubo = *frame_resrc.point_light_ubo;

  const auto cam_set = desc_pool_.GetCameraSets().GetSet(frame_idx);
  const auto gbuff_set = desc_pool_.GetGBuffSets().GetSet(frame_idx);
  const auto point_set = desc_pool_.GetPointLightSets().GetSet(frame_idx);

  uint32_t point_at{0};
  scene.GetPointLightQuery().each(
      [&](const PointLightTag&, const TransformComp& tf, const LightComp& light,
          const RangeComp& range, BoundingBoxComp& bb) {
        if (point_at >= kMaxPointLights || !frustum.IsVisible(bb)) return;

        LightPushConst light_pc{tf.glob_mat, settings_.diff_int,
                                settings_.spec_int, settings_.is_pbr};

        glm::vec3 world_pos = glm::vec3(tf.glob_mat[3]);
        glm::vec3 view_pos = cam_ubo.view * glm::vec4(world_pos, 1.0f);

        point_ubo.Write(
            point_at, PointLightUnif{.pos = glm::vec4(view_pos, range.range),
                                     .color = glm::vec4(
                                         light.color * light.intensity, 1.0f)});

        vk::DeviceSize offset = 0;
        const auto vbo = sphere_mesh.vbo.GetBuffer();

        // === BEGIN RENDER PASS FOR POINT LIGHT ===
        cmd_buff.beginRenderPass(
            passes_.local_light_.BeginInfo(frame_idx, resrc_extent),
            vk::SubpassContents::eInline);

        {  // write stencil
          cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                pipelines_.local_light_.GetPipeline());

          cmd_buff.bindDescriptorSets(  // camera
              vk::PipelineBindPoint::eGraphics,
              pipelines_.local_light_.GetLayout(), 0, cam_set, {});

          cmd_buff.pushConstants(pipelines_.local_light_.GetLayout(),
                                 vk::ShaderStageFlagBits::eVertex, 0,
                                 sizeof(light_pc), &light_pc);

          cmd_buff.bindVertexBuffers(0, 1, &vbo, &offset);
          cmd_buff.bindIndexBuffer(sphere_mesh.ibo.GetBuffer(), 0,
                                   vk::IndexType::eUint32);

          cmd_buff.drawIndexed(sphere_mesh.ibo.GetCount(), 1, 0, 0, 0);
        }

        cmd_buff.nextSubpass(vk::SubpassContents::eInline);

        {  // render point light

          cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                pipelines_.point_light_.GetPipeline());

          cmd_buff.bindDescriptorSets(  // camera
              vk::PipelineBindPoint::eGraphics,
              pipelines_.point_light_.GetLayout(), 0, cam_set, {});

          cmd_buff.bindDescriptorSets(  // gbuff
              vk::PipelineBindPoint::eGraphics,
              pipelines_.point_light_.GetLayout(), 1, gbuff_set, {});

          cmd_buff.bindDescriptorSets(
              vk::PipelineBindPoint::eGraphics,  // point light
              pipelines_.point_light_.GetLayout(), 2, point_set,
              point_ubo.GetElemOffset(point_at));

          cmd_buff.pushConstants(pipelines_.point_light_.GetLayout(),
                                 vk::ShaderStageFlagBits::eVertex |
                                     vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(light_pc), &light_pc);

          cmd_buff.bindVertexBuffers(0, 1, &vbo, &offset);
          cmd_buff.bindIndexBuffer(sphere_mesh.ibo.GetBuffer(), 0,
                                   vk::IndexType::eUint32);

          cmd_buff.drawIndexed(sphere_mesh.ibo.GetCount(), 1, 0, 0, 0);
        }
        cmd_buff.endRenderPass();

        point_at++;
      });
}

void Renderer::RecordSpotLights(vk::CommandBuffer cmd_buff,
                                const uint frame_idx,
                                const npr_graphics::FrameResources& frame_resrc,
                                const vk::Extent2D& resrc_extent,
                                const CameraUnif& cam_ubo,
                                const npr_scene::Camera& camera,
                                npr_scene::Scene& scene) {
  const auto& frustum = camera.GetFrustum();
  const auto& cone_mesh = resrc_.GetConeMesh();
  auto& spot_ubo = *frame_resrc.spot_light_ubo;

  const auto cam_set = desc_pool_.GetCameraSets().GetSet(frame_idx);
  const auto gbuff_set = desc_pool_.GetGBuffSets().GetSet(frame_idx);
  const auto spot_set = desc_pool_.GetSpotLightSets().GetSet(frame_idx);

  uint32_t spot_at{0};
  scene.GetSpotLightQuery().each(
      [&](const SpotLightTag&, const TransformComp& tf, const LightComp& light,
          const RangeComp& range, const SpotComp& spot, BoundingBoxComp& bb) {
        if (spot_at >= kMaxSpotLights || !frustum.IsVisible(bb)) return;

        LightPushConst light_pc{tf.glob_mat, settings_.diff_int,
                                settings_.spec_int, settings_.is_pbr};

        glm::vec3 world_pos = glm::vec3(tf.glob_mat[3]);
        glm::vec3 view_pos = cam_ubo.view * glm::vec4(world_pos, 1.0f);

        glm::mat3 world_rot = glm::mat3(tf.glob_mat);
        world_rot[0] = glm::normalize(world_rot[0]);
        world_rot[1] = glm::normalize(world_rot[1]);
        world_rot[2] = glm::normalize(world_rot[2]);

        glm::vec3 world_dir = world_rot * glm::vec3(0, 0, -1);
        glm::vec3 view_dir =
            glm::normalize(glm::mat3(cam_ubo.view) * world_dir);

        float inner = glm::cos(spot.inner_cone_angle);
        float outer = glm::cos(spot.outer_cone_angle);
        float angle_scale = 1.0f / std::max(0.001f, inner - outer);
        float angle_offset = -outer * angle_scale;

        spot_ubo.Write(
            spot_at,
            SpotLightUnif{
                .pos = glm::vec4(view_pos, range.range),
                .dir = glm::vec4(-view_dir, 0.0f),
                .color = glm::vec4(light.color * light.intensity, 1.0f),
                .params = glm::vec4(angle_scale, angle_offset, 0.0f, 0.0f)});

        vk::DeviceSize offset = 0;
        const auto vbo = cone_mesh.vbo.GetBuffer();

        // === BEGIN RENDER PASS FOR SPOT LIGHT ===
        cmd_buff.beginRenderPass(
            passes_.local_light_.BeginInfo(frame_idx, resrc_extent),
            vk::SubpassContents::eInline);

        {  // write stencil
          cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                pipelines_.local_light_.GetPipeline());

          cmd_buff.bindDescriptorSets(  // camera
              vk::PipelineBindPoint::eGraphics,
              pipelines_.local_light_.GetLayout(), 0, cam_set, {});

          cmd_buff.pushConstants(pipelines_.local_light_.GetLayout(),
                                 vk::ShaderStageFlagBits::eVertex, 0,
                                 sizeof(light_pc), &light_pc);

          cmd_buff.bindVertexBuffers(0, 1, &vbo, &offset);
          cmd_buff.bindIndexBuffer(cone_mesh.ibo.GetBuffer(), 0,
                                   vk::IndexType::eUint32);

          cmd_buff.drawIndexed(cone_mesh.ibo.GetCount(), 1, 0, 0, 0);
        }

        cmd_buff.nextSubpass(vk::SubpassContents::eInline);

        {  // render spot light
          cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                pipelines_.spot_light_.GetPipeline());

          cmd_buff.bindDescriptorSets(  // camera
              vk::PipelineBindPoint::eGraphics,
              pipelines_.spot_light_.GetLayout(), 0, cam_set, {});

          cmd_buff.bindDescriptorSets(  // gbuff
              vk::PipelineBindPoint::eGraphics,
              pipelines_.spot_light_.GetLayout(), 1, gbuff_set, {});

          cmd_buff.bindDescriptorSets(
              vk::PipelineBindPoint::eGraphics,  // spot light
              pipelines_.spot_light_.GetLayout(), 2, spot_set,
              spot_ubo.GetElemOffset(spot_at));

          cmd_buff.pushConstants(pipelines_.spot_light_.GetLayout(),
                                 vk::ShaderStageFlagBits::eVertex |
                                     vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(light_pc), &light_pc);

          cmd_buff.bindVertexBuffers(0, 1, &vbo, &offset);
          cmd_buff.bindIndexBuffer(cone_mesh.ibo.GetBuffer(), 0,
                                   vk::IndexType::eUint32);

          cmd_buff.drawIndexed(cone_mesh.ibo.GetCount(), 1, 0, 0, 0);
        }
        cmd_buff.endRenderPass();

        spot_at++;
      });
}

void Renderer::RecordABuff(vk::CommandBuffer cmd_buff, const uint frame_idx,
                           const npr_graphics::FrameResources& frame_resrc,
                           const vk::Extent2D& resrc_extent,
                           const npr_scene::Camera& camera, Scene& scene) {
  // clear abuff before fill pass
  const uint32_t null_ptr = 0xFFFFFFFF;

  cmd_buff.fillBuffer(frame_resrc.abuff_heads->GetBuffer(), 0, VK_WHOLE_SIZE,
                      null_ptr);
  cmd_buff.fillBuffer(frame_resrc.abuff_counter->GetBuffer(), 0, VK_WHOLE_SIZE,
                      0);

  cmd_buff.beginRenderPass(passes_.abuff_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  {  // fill
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          pipelines_.abuff_fill_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // camera
        vk::PipelineBindPoint::eGraphics, pipelines_.abuff_fill_.GetLayout(), 0,
        desc_pool_.GetCameraSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // textures
        vk::PipelineBindPoint::eGraphics, pipelines_.abuff_fill_.GetLayout(), 1,
        scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

    cmd_buff.bindDescriptorSets(  // abuff
        vk::PipelineBindPoint::eGraphics, pipelines_.abuff_fill_.GetLayout(), 2,
        desc_pool_.GetABufferSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // dir lights
        vk::PipelineBindPoint::eGraphics, pipelines_.abuff_fill_.GetLayout(), 3,
        desc_pool_.GetDirLightSets().GetSet(frame_idx), {});

    ABuffFillPushConst pc{.width = resrc_extent.width,
                          .max_nodes = resrc_.frame_props_.abuff_max_nodes,
                          .alpha_cutoff = settings_.alpha_cutoff,
                          .diff_int = settings_.diff_int,
                          .spec_int = settings_.spec_int,
                          .is_pbr = settings_.is_pbr};

    cmd_buff.pushConstants(pipelines_.abuff_fill_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0, sizeof(pc),
                           &pc);

    RecordTrans(cmd_buff, frame_idx, frame_resrc, scene, camera.GetFrustum(),
                pipelines_.abuff_fill_.GetLayout(), 4);
  }

  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  {  // resolve
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          pipelines_.abuff_res_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // abuff
        vk::PipelineBindPoint::eGraphics, pipelines_.abuff_res_.GetLayout(), 0,
        desc_pool_.GetABufferSets().GetSet(frame_idx), {});

    ABuffResPushConst pc{.width = resrc_extent.width,
                         .sorted_nodes = settings_.abuff_sorted_nodes};

    cmd_buff.pushConstants(pipelines_.abuff_res_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0, sizeof(pc),
                           &pc);

    cmd_buff.draw(3, 1, 0, 0);
  }

  cmd_buff.endRenderPass();
}

void Renderer::RecordWBoit(vk::CommandBuffer cmd_buff, const uint frame_idx,
                           const npr_graphics::FrameResources& frame_resrc,
                           const vk::Extent2D& resrc_extent,
                           const npr_scene::Camera& camera, Scene& scene) {
  cmd_buff.beginRenderPass(passes_.wboit_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  {  // acc
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          pipelines_.wboit_acc_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // camera
        vk::PipelineBindPoint::eGraphics, pipelines_.wboit_acc_.GetLayout(), 0,
        desc_pool_.GetCameraSets().GetSet(frame_idx), {});

    cmd_buff.bindDescriptorSets(  // textures
        vk::PipelineBindPoint::eGraphics, pipelines_.wboit_acc_.GetLayout(), 1,
        scene.GetResrc().desc_pool.GetTextureSet().GetSet(0), {});

    cmd_buff.bindDescriptorSets(  // dir lights
        vk::PipelineBindPoint::eGraphics, pipelines_.wboit_acc_.GetLayout(), 2,
        desc_pool_.GetDirLightSets().GetSet(frame_idx), {});

    WBoitPushConst pc{.alpha_multiplier = settings_.wboit_alpha_multiplier,
                      .alpha_power = settings_.wboit_alpha_power,
                      .depth_factor = settings_.wboit_depth_factor,
                      .depth_power = settings_.wboit_depth_power,
                      .weight_min = settings_.wboit_weight_min,
                      .weight_max = settings_.wboit_weight_max,
                      .alpha_cutoff = settings_.alpha_cutoff,
                      .diff_int = settings_.diff_int,
                      .spec_int = settings_.spec_int,
                      .is_pbr = settings_.is_pbr};

    cmd_buff.pushConstants(pipelines_.wboit_acc_.GetLayout(),
                           vk::ShaderStageFlagBits::eFragment, 0, sizeof(pc),
                           &pc);

    RecordTrans(cmd_buff, frame_idx, frame_resrc, scene, camera.GetFrustum(),
                pipelines_.wboit_acc_.GetLayout(), 3);
  }

  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  {  // resolve
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                          pipelines_.wboit_res_.GetPipeline());

    cmd_buff.bindDescriptorSets(  // input attachments
        vk::PipelineBindPoint::eGraphics, pipelines_.wboit_res_.GetLayout(), 0,
        desc_pool_.GetWBoitInputSets().GetSet(frame_idx), {});

    cmd_buff.draw(3, 1, 0, 0);
  }

  cmd_buff.endRenderPass();
}

void Renderer::RecordTrans(vk::CommandBuffer cmd_buff, const uint frame_idx,
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

    if (inst_at + visible.size() >= kMaxInstances) inst_at = 0;
    frame_resrc.instance_buff->Write(visible, inst_at);

    std::array<vk::DeviceSize, 2> offsets = {0, inst_at * sizeof(InstanceData)};
    std::array<vk::Buffer, 2> buffers = {
        scene_resrc.vbos[key.mesh.vbo_idx].GetBuffer(),
        frame_resrc.instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      cmd_buff.bindIndexBuffer(scene_resrc.ibos[key.mesh.ibo_idx].GetBuffer(),
                               0, vk::IndexType::eUint32);
      cmd_buff.drawIndexed(scene_resrc.ibos[key.mesh.ibo_idx].GetCount(),
                           visible.size(), 0, 0, 0);
    } else {
      cmd_buff.draw(scene_resrc.vbos[key.mesh.vbo_idx].GetCount(),
                    visible.size(), 0, 0);
    }

    mat_at = (mat_at + 1) % kMaxMaterials;
    inst_at = (inst_at + visible.size()) % kMaxInstances;
  }
}

void Renderer::RecordBloom(vk::CommandBuffer cmd_buff, const uint frame_idx,
                           const vk::Extent2D& resrc_extent) {
  // extract bright
  cmd_buff.beginRenderPass(
      passes_.bright_extract_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.bright_.GetPipeline());

  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              pipelines_.bright_.GetLayout(), 0,
                              desc_pool_.GetColorSets().GetSet(frame_idx), {});

  BrightPushConst bright_pc{.threshold = settings_.bloom_threshold,
                            .soft_threshold = settings_.bloom_soft_threshold,
                            .intensity = settings_.bloom_intensity};

  cmd_buff.pushConstants(pipelines_.bright_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(bright_pc), &bright_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // horizontal blur pass
  cmd_buff.beginRenderPass(
      passes_.blur_bright_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.blur_bright_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // bright_color
      vk::PipelineBindPoint::eGraphics, pipelines_.blur_bright_.GetLayout(), 0,
      desc_pool_.GetBrightColorSets().GetSet(frame_idx), {});

  auto& blur_pc = resrc_.GetBloomBlurPC();
  blur_pc.flags.x = 0;  // horizontal
  cmd_buff.pushConstants(pipelines_.blur_bright_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(blur_pc),
                         &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // vertical blur pass
  cmd_buff.beginRenderPass(
      passes_.bright_extract_.BeginInfo(frame_idx, resrc_extent),
      vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.blur_bright_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // bright_temp
      vk::PipelineBindPoint::eGraphics, pipelines_.blur_bright_.GetLayout(), 0,
      desc_pool_.GetBrightTempSets().GetSet(frame_idx), {});

  blur_pc.flags.x = 1;  // vertical
  cmd_buff.pushConstants(pipelines_.blur_bright_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(blur_pc),
                         &blur_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();
}

void Renderer::RecordDoF(vk::CommandBuffer cmd_buff, const uint frame_idx,
                         const vk::Extent2D& resrc_extent,
                         const npr_scene::Camera& camera) {
  // calculate coc map
  cmd_buff.beginRenderPass(passes_.coc_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.coc_.GetPipeline());

  cmd_buff.bindDescriptorSets(  // depth_tex
      vk::PipelineBindPoint::eGraphics, pipelines_.coc_.GetLayout(), 0,
      desc_pool_.GetDepthSets().GetSet(frame_idx), {});

  CocPushConst coc_pc{};
  coc_pc.focus_dist = settings_.dof_focus_distance;
  coc_pc.focus_range = settings_.dof_focus_range;
  coc_pc.near_int = settings_.dof_near_int;
  coc_pc.far_int = settings_.dof_far_int;
  coc_pc.near_falloff = settings_.dof_near_falloff;
  coc_pc.far_falloff = settings_.dof_far_falloff;
  coc_pc.near_plane = camera.GetNear();
  coc_pc.far_plane = camera.GetFar();
  coc_pc.is_persp = !camera.IsOrtho();

  cmd_buff.pushConstants(pipelines_.coc_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(coc_pc),
                         &coc_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();

  // dof poisson blur
  cmd_buff.beginRenderPass(passes_.dof_.BeginInfo(frame_idx, resrc_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.dof_.GetPipeline());

  // color_tex
  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              pipelines_.dof_.GetLayout(), 0,
                              desc_pool_.GetColorSets().GetSet(frame_idx), {});

  // coc_map
  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              pipelines_.dof_.GetLayout(), 1,
                              desc_pool_.GetCocMapSets().GetSet(frame_idx), {});

  // blue_noise + poisson
  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              pipelines_.dof_.GetLayout(), 2,
                              desc_pool_.GetDofSet().GetSet(), {});

  DofPushConst dof_pc{};
  dof_pc.blur_radius = settings_.dof_blur_radius;
  dof_pc.coc_threshold = settings_.dof_coc_threshold;
  dof_pc.coc_falloff = settings_.dof_coc_falloff;
  dof_pc.debug_mode = settings_.dof_debug_mode;

  cmd_buff.pushConstants(pipelines_.dof_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0, sizeof(dof_pc),
                         &dof_pc);

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.endRenderPass();
}

void Renderer::RecordSwap(vk::CommandBuffer cmd_buff, uint image_idx,
                          const uint frame_idx, const npr_scene::Camera& camera,
                          bool is_loading, float dt) {
  auto swap_extent = swapchain_.GetProps().extent;
  auto [viewport, scissor] = CalcViewportScissor(swap_extent, camera);

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

  cmd_buff.beginRenderPass(passes_.swap_.BeginInfo(image_idx, swap_extent),
                           vk::SubpassContents::eInline);

  cmd_buff.setScissor(0, scissor);
  cmd_buff.setViewport(0, viewport);

  cmd_buff.pushConstants(pipelines_.swap_.GetLayout(),
                         vk::ShaderStageFlagBits::eFragment, 0,
                         sizeof(load_data), &load_data);
  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        pipelines_.swap_.GetPipeline());

  if (settings_.enable_dof) {
    cmd_buff.bindDescriptorSets(  // present color
        vk::PipelineBindPoint::eGraphics, pipelines_.swap_.GetLayout(), 0,
        desc_pool_.GetPresentColorSets().GetSet(frame_idx), {});
  } else {
    cmd_buff.bindDescriptorSets(  // color_res
        vk::PipelineBindPoint::eGraphics, pipelines_.swap_.GetLayout(), 0,
        desc_pool_.GetColorSets().GetSet(frame_idx), {});
  }

  cmd_buff.draw(3, 1, 0, 0);
  cmd_buff.nextSubpass(vk::SubpassContents::eInline);

  gui_.RecordUI(cmd_buff);

  cmd_buff.endRenderPass();
}

std::pair<vk::Viewport, vk::Rect2D> Renderer::CalcViewportScissor(
    vk::Extent2D extent, const npr_scene::Camera& camera) const {
  if (!camera.IsFocused()) {
    auto viewport =
        vk::Viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f);
    auto scissor = vk::Rect2D{{0, 0}, extent};
    return {viewport, scissor};
  }

  float win_aspect = static_cast<float>(extent.width) / extent.height;
  float cam_aspect = camera.GetAspect();

  if (win_aspect > cam_aspect) {
    // window is wider: letterbox (black bars on left/right)
    uint32_t width = extent.height * cam_aspect;
    int32_t offset = (extent.width - width) / 2;

    vk::Viewport viewport(offset, 0, width, extent.height, 0.0f, 1.0f);
    vk::Rect2D scissor{{offset, 0}, {width, extent.height}};

    return {viewport, scissor};
  } else {
    // window is taller: pillarbox (black bars on top/bottom)
    uint32_t height = extent.width / cam_aspect;
    int32_t offset = (extent.height - height) / 2;

    vk::Viewport viewport(0, offset, extent.width, height, 0.0f, 1.0f);
    vk::Rect2D scissor{{0, offset}, {extent.width, height}};

    return {viewport, scissor};
  }
}

}  // namespace npr_graphics
