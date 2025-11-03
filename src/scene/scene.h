#ifndef SCENE_H_
#define SCENE_H_

#include "core/task_manager.h"
#include "graphics/command_pool.h"
#include "graphics/descriptor_pool.h"
#include "graphics/buffer.h"
#include "graphics/image.h"

#include <flecs.h>

namespace npr_scene {
struct GpuResources {
  const npr_graphics::VulkanContext& context;

  npr_graphics::CommandPool cmd_pool;
  vk::CommandBuffer cmd_buff{nullptr};

  npr_graphics::TexDescriptorPool desc_pool;

  std::vector<npr_graphics::IndexBuffer> ibos;
  std::vector<npr_graphics::VertexBuffer> vbos;
  std::vector<npr_graphics::Texture> textures;

  GpuResources(const npr_graphics::VulkanContext& context,
               const std::string& dbg_name)
      : context{context},
        cmd_pool{context, 1,
                 vk::CommandPoolCreateFlagBits::eTransient |
                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                 context.GetQFamilies().graphics_i.value(),
                 "cmd_pool_" + dbg_name},
        cmd_buff{cmd_pool.GetCmdBuff()},
        desc_pool{context} {}

  void UpdateTextureDescriptors() { desc_pool.Update(textures); }
  void Reset() {
    ibos.clear();
    vbos.clear();
    textures.clear();

    cmd_pool.GetCmdBuff().reset(
        vk::CommandBufferResetFlagBits::eReleaseResources);
  }
};

class Scene : npr_core::NonCopyable {
  friend class SceneLoader;

 public:
  Scene() = default;
  ~Scene() { WaitForAsync(); }

  Scene& operator=(Scene&& scene) noexcept;

  const std::string& GetSceneName() { return scene_name_; }
  const std::string& GetFilename() { return filename_; }
  GpuResources& GetGpuResources() {
    assert(gpu_res_);
    return *gpu_res_;
  }

  void InitGPU(npr_core::TaskManager& tasks, std::function<void()> on_complete);

  bool IsLoading();
  bool IsValid() { return !IsLoading() && valid_; };
  bool IsInit() const { return gpu_init_; }

 private:
  void Prepare(const npr_graphics::VulkanContext& context,
               const std::string& filepath, const std::string& scene_name);

  void WaitForAsync();

 private:
  flecs::world entities_;
  std::unique_ptr<GpuResources> gpu_res_;

  std::string filename_;
  std::string scene_name_;

  bool valid_{false};     // scene loaded and valid
  bool gpu_init_{false};  // gpu resources initialized

  std::future<bool> handle_;
};
}  // namespace npr_scene

#endif  // SCENE_H_
