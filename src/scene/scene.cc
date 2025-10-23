#include "scene.h"

namespace npr_scene {
bool Scene::IsLoading() {
  if (!handle_.valid()) return false;
  if (handle_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return true;

  valid_ = handle_.get();
  return false;
}

}  // namespace npr_scene
