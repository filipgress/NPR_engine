#include "scene.h"

namespace npr_scene {

Scene& Scene::operator=(Scene&& scene) noexcept {
  if (this == &scene) return *this;

  assert(!IsLoading());

  world_ = std::move(scene.world_);
  gpu_res_ = std::move(scene.gpu_res_);
  valid_ = std::exchange(scene.valid_, false);
  handle_ = std::move(scene.handle_);

  return *this;
}

bool Scene::IsLoading() {
  if (!handle_.valid()) return false;
  if (handle_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return true;

  valid_ = handle_.get();
  gpu_init_ = false;

  return false;
}

void Scene::Prepare(const npr_graphics::Context& ctx,
                    const std::string& filepath,
                    const std::string& scene_name) {
  filename_ = npr_core::GetFilename(filepath);
  scene_name_ = scene_name.empty() ? "default" : scene_name;

  world_.Reset();
  if (gpu_res_ && gpu_res_->ctx.GetDevice() == ctx.GetDevice())
    gpu_res_->Reset();
  else
    gpu_res_ = std::make_unique<GpuResources>(ctx, "scene_" + scene_name_);
}

void Scene::InitGPU(npr_core::TaskManager& tasks,
                    std::function<void()> on_complete) {
  if (!IsValid() || gpu_init_) return;

  auto device = gpu_res_->ctx.GetDevice();
  auto cmd_buff = gpu_res_->cmd_pool.GetCmdBuff();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buff;

  vk::Fence fence = device.createFence({});
  gpu_res_->ctx.GetGraphicsQ().submit(submit_info, fence);

  tasks.Add([this, on_complete, device, fence]() {
    auto status = device.getFenceStatus(fence);

    if (status == vk::Result::eNotReady) return false;
    device.destroyFence(fence);

    if (status == vk::Result::eErrorDeviceLost) {
      ERR("error during scene loading:", vk::to_string(status));
      return true;
    }

    INFO("scene initialized: ", filename_, "(", scene_name_, ")");

    world_.BuildQueries();
    world_.Update();

    gpu_init_ = true;
    gpu_res_->DestroyStagingBuffers();

    on_complete();
    return true;
  });
}

void Scene::WaitForAsync() {
  if (!handle_.valid()) return;

  valid_ = handle_.get();
  gpu_init_ = false;
}

void Scene::RecordOpaque(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
                         const npr_graphics::FrameResources& frame_res,
                         const uint32_t set_idx, vk::DescriptorSet material_set,
                         const Frustum& frustum) const {
  world_.Record(
      cmd_buff, layout, frame_res, set_idx, material_set, frustum, *gpu_res_,
      true, [](const MeshMaterialKey& key) {
        return key.material.flags & npr_graphics::MaterialFlags::kOpaque ||
               key.material.flags & npr_graphics::MaterialFlags::kMask;
      });
}

void Scene::RecordTrans(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
                        const npr_graphics::FrameResources& frame_res,
                        const uint32_t set_idx, vk::DescriptorSet material_set,
                        const Frustum& frustum) const {
  world_.Record(
      cmd_buff, layout, frame_res, set_idx, material_set, frustum, *gpu_res_,
      false, [](const MeshMaterialKey& key) {
        return !(key.material.flags & npr_graphics::MaterialFlags::kOpaque) &&
               !(key.material.flags & npr_graphics::MaterialFlags::kMask);
      });
}

}  // namespace npr_scene
