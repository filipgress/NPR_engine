#include "app.h"

#include "window/app_event.h"
#include "scene/scene_loader.h"

namespace npr_core {

App::App()
    : active_scene_{std::make_unique<npr_scene::Scene>()},
      scene_swap_{std::make_unique<npr_scene::Scene>()} {
  npr_scene::SceneLoader::LoadAsync(
      renderer_.GetContext(), *scene_swap_, tasks_,
      [this]() { scene_swap_.swap(active_scene_); },
      "../assets/market/scene.gltf");

  tasks_.Add([&]() {
    if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
        inputs_.key_tokens.contains(GLFW_KEY_R))
      renderer_.RecompileShaders();

    if (inputs_.key_tokens.contains(GLFW_KEY_I))
      INFO(timer_.GetAvgFPS(), "fps");

    return false;
  });
}

void App::Run() {
  while (running_) {
    timer_.Update();

    window_.PollEvents();
    inputs_.Update(window_.GetSize());
    tasks_.Process();

    if (window_.IsMinimized()) continue;
    renderer_.Render(camera_, *active_scene_, scene_swap_->IsLoading(),
                     timer_.GetElapsed());
  }

  renderer_.Finish();
}

void App::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // app events
  dispatcher.Dispatch<AppTickEvent>([this](AppTickEvent&) {
    renderer_.SwapShaders();
    return true;
  });

  // window events
  dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
    running_ = false;
    return true;
  });
  dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent&) {
    renderer_.Resize();
    camera_.SetAspect(window_.GetAspect());

    return true;
  });

  if (e.IsHandled()) return;
  inputs_.OnEvent(e);
}

}  // namespace npr_core
