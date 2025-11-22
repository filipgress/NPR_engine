#include "scene_resrc.h"

namespace npr_scene {

SceneResrc::SceneResrc(const npr_graphics::Context& ctx,
                       const std::string& dbg_name)
    : ctx{ctx},
      cmd_pool{ctx, 1, dbg_name},
      cmd_buff{cmd_pool.GetCmdBuff()},
      desc_pool{ctx, dbg_name} {}

void SceneResrc::Submit(npr_core::TaskManager& tasks,
                        std::function<void()> on_complete) {
  auto device = ctx.GetDevice();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buff;

  vk::Fence fence = device.createFence({});
  ctx.GetGraphicsQ().submit(submit_info, fence);

  tasks.Add([this, on_complete, device, fence]() {
    auto status = device.getFenceStatus(fence);

    if (status == vk::Result::eNotReady) return false;

    device.destroyFence(fence);
    DestroyStagingBuffs();

    if (status == vk::Result::eErrorDeviceLost) {
      ERR("unable to initialize scene gpu resources: ", vk::to_string(status));
      return true;
    }

    INFO("scene gpu resources initialized");

    init_ = true;
    on_complete();

    return true;
  });
}

void SceneResrc::Reset() {
  init_ = false;

  ibos.clear();
  vbos.clear();
  textures.clear();

  cmd_pool.GetCmdBuff().reset(
      vk::CommandBufferResetFlagBits::eReleaseResources);
}

void SceneResrc::DestroyStagingBuffs() {
  for (auto& ibo : ibos) ibo.DestroyStagingBuff();
  for (auto& vbo : vbos) vbo.DestroyStagingBuff();
  for (auto& texture : textures) texture.DestroyStagingBuff();
}

}  // namespace npr_scene
