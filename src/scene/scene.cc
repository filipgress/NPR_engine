#include "scene.h"

namespace npr_scene {

bool Scene::IsLoading() const {
  return handle_.valid() &&
         handle_.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void Scene::StopLoading() {
  if (!handle_.valid()) return;

  stop_async_ = true;
  success_ = handle_.get();
}

void Scene::Init(npr_core::TaskManager& tasks,
                 std::function<void()> on_complete) {
  world_.BuildQueries();
  world_.Update();
  resrc_->Submit(tasks, on_complete);
}

}  // namespace npr_scene
