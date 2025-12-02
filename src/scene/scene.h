#ifndef SCENE_H_
#define SCENE_H_

#include "world.h"
#include "scene_resrc.h"

#include "core/task_manager.h"

namespace npr_scene {
class Scene : npr_core::NonCopyable {
  friend class SceneLoader;

 public:
  Scene() = default;
  ~Scene() { StopLoading(); }

  // !! getters can only be used on valid scenes !!
  const std::string& GetSceneName() const { return scene_name_; }
  const std::string& GetFilename() const { return filename_; }

  auto& GetResrc() const { return *resrc_; }
  auto& GetInstances() const { return world_.instances_; }

  auto& GetCamQuery() const { return world_.cam_query_; }
  auto& GetObjectQuery() const { return world_.object_query_; }
  auto& GetRenderableQuery() const { return world_.renderable_query_; }
  auto& GetDirLightQuery() const { return world_.dir_light_query_; }
  auto& GetPointLightQuery() const { return world_.point_light_query_; }
  auto& GetSpotLightQuery() const { return world_.spot_light_query_; }

  void Update() {
    if (success_) world_.Update();
  }

  void Invalidate() { success_ = false; }

  bool IsLoading() const { return handle_.valid(); }
  bool IsValid() const { return success_; }
  bool IsInit() const { return success_ && resrc_->IsInit(); }

 private:
  void Init(npr_core::TaskManager& tasks, std::function<void()> on_complete);
  void StopLoading();

  bool IsReady() const;

 private:
  World world_;
  std::unique_ptr<SceneResrc> resrc_;

  std::string filename_;
  std::string scene_name_;

  bool success_{false};

  std::future<bool> handle_;
  std::atomic<bool> stop_async_{false};
};
}  // namespace npr_scene

#endif  // SCENE_H_
