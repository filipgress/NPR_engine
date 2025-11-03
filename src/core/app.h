#ifndef APP_H_
#define APP_H_

#include "frame_timer.h"
#include "input_handler.h"
#include "task_manager.h"

#include "window/window.h"
#include "graphics/renderer.h"

#include "scene/scene.h"
#include "scene/camera.h"

namespace npr_core {
class App : public NonCopyable {
 public:
  App();
  void Run();

 private:
  void OnEvent(npr_window::Event& e);

 private:
  npr_window::Window window_{[this](npr_window::Event& e) { OnEvent(e); }};
  npr_graphics::Renderer renderer_{window_};

  npr_scene::Camera camera_{window_.GetAspect()};
  std::unique_ptr<npr_scene::Scene> active_scene_;
  std::unique_ptr<npr_scene::Scene> scene_swap_;

  FrameTimer timer_{0, [this](npr_window::Event& e) { return OnEvent(e); }};
  InputHandler inputs_;
  TaskManager tasks_;

  bool running_{true};
};

}  // namespace npr_core

#endif  // APP_H_
