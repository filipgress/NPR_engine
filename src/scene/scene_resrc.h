#ifndef SCENE_RESRC_H_
#define SCENE_RESRC_H_

#include "core/task_manager.h"

#include "graphics/context.h"
#include "graphics/command_pool.h"
#include "graphics/descriptor_pool.h"

namespace npr_scene {

struct SceneResrc {
  SceneResrc(const npr_graphics::Context& ctx, const std::string& dbg_name);

  bool IsInit() const { return init_; }
  void Submit(npr_core::TaskManager& tasks, std::function<void()> on_complete);

  void UpdateTexDesc(const npr_graphics::Texture& default_tex) {
    desc_pool.GetTextureSet().Update(textures, default_tex);
  }
  bool Compatible(const vk::Device& device) const {
    return ctx.GetDevice() == device;
  }

  void Reset();
  void DestroyStagingBuffs();

  const npr_graphics::Context& ctx;

  npr_graphics::GraphicsCommandPool cmd_pool;
  vk::CommandBuffer cmd_buff;

  npr_graphics::TexDescriptorPool desc_pool;

  std::vector<npr_graphics::IndexBuffer> ibos;
  std::vector<npr_graphics::VertexBuffer> vbos;
  std::vector<npr_graphics::Texture> textures;

  bool init_{false};
};

}  // namespace npr_scene

#endif  // SCENE_RESRC_H_
