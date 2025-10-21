#ifndef RESOURCES_H_
#define RESOURCES_H_

#include "vulkan_context.h"
#include "buffer.h"

#include "scene/scene.h"

namespace npr_graphics {

struct SceneResources {
  std::vector<npr_graphics::IndexBuffer> index_buffs;
  std::vector<npr_graphics::VertexBuffer> vertex_buffs;
  // std::vector<Textures> textures;
};

struct FrameProps {};
struct FrameResources {};

class Resources : public npr_core::NonCopyable {
 public:
  Resources(const VulkanContext& context);

  const std::vector<FrameResources>& GetResources() const {
    return frame_resources_;
  }

  // bind scenes that have finished loading for rendering
  void Bind(npr_scene::Scene&& scene) { scene_ = std::move(scene); }

 private:
  const VulkanContext& c_;

  FrameProps frame_props_;
  std::vector<FrameResources> frame_resources_;

  npr_scene::Scene scene_;
};

}  // namespace npr_graphics

#endif  // RESOURCES_H_
