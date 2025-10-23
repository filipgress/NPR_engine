#ifndef SCENE_H_
#define SCENE_H_

#include <flecs.h>

#include "graphics/buffer.h"
#include "graphics/command_pool.h"
#include "graphics/descriptor_pool.h"

namespace npr_scene {
struct GpuResources {
  std::vector<npr_graphics::IndexBuffer> ibos;
  std::vector<npr_graphics::VertexBuffer> vbos;
  // std::vector<Textures> textures;

  std::unique_ptr<npr_graphics::CommandPool> cmd_pool;
  std::unique_ptr<npr_graphics::DescriptorPool> desc_pool;
};

class Scene : npr_core::NonCopyable {
  friend class SceneLoader;

 public:
  Scene() = default;
  ~Scene() { WaitForAsync(); }

  Scene& operator=(Scene&& scene) noexcept {
    if (this == &scene) return *this;

    assert(!IsLoading());

    entities_ = std::move(scene.entities_);
    gpu_resources_ = std::move(scene.gpu_resources_);
    valid_ = std::exchange(scene.valid_, false);
    handle_ = std::move(scene.handle_);

    return *this;
  }

  const std::string& GetName() { return name_; }
  const GpuResources& GetGpuResources() const { return gpu_resources_; }

  bool IsLoading();
  bool IsValid() { return !IsLoading() && valid_; };

 private:
  void Clear() {
    entities_.reset();
    gpu_resources_.ibos.clear();
    gpu_resources_.vbos.clear();
  }

  void WaitForAsync() {
    if (handle_.valid()) valid_ = handle_.get();
  }

 private:
  std::string name_;

  flecs::world entities_;
  GpuResources gpu_resources_;

  bool valid_{false};
  std::future<bool> handle_;
};
}  // namespace npr_scene

#endif  // SCENE_H_
