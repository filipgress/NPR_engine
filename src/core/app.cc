#include "app.h"

#include "window/app_event.h"
#include "scene/scene_loader.h"

namespace npr_core {

App::App()
    : active_scene_{std::make_unique<npr_scene::Scene>()},
      scene_swap_{std::make_unique<npr_scene::Scene>()} {
  npr_scene::SceneLoader::LoadAsync(
      renderer_, *scene_swap_, tasks_,
      [this]() { scene_swap_.swap(active_scene_); },
      // "../assets/scene_graph/scene.gltf");
      "../assets/market/scene.gltf");

  tasks_.Add([&]() {
    if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
        inputs_.key_tokens.contains(GLFW_KEY_R))
      renderer_.RecompileShaders();

    if (inputs_.key_tokens.contains(GLFW_KEY_I))
      INFO(timer_.GetAvgFPS(), "fps");

    if (inputs_.key_tokens.contains(GLFW_KEY_O)) {
      if (active_scene_->IsValid()) {
        flecs::entity first_camera = active_scene_->GetCameraQuery().first();
        // active_scene_->GetCameraQuery().run([&](flecs::iter it) {
        //   while (it.next()) first_camera = it.entity(0);
        // });
        camera_.SetEntity(first_camera);
      }
    }

    if (camera_.GetMode() == npr_scene::CameraMode::kFree) {
      float dt = timer_.GetDelta();
      camera_.Rotate(inputs_.mouse_move, dt);

      if (inputs_.key_tokens.contains(GLFW_KEY_W))
        camera_.Move({0.0f, 0.0f, 1.0f}, dt);
      if (inputs_.key_tokens.contains(GLFW_KEY_A))
        camera_.Move({-1.0f, 0.0f, 0.0f}, dt);
      if (inputs_.key_tokens.contains(GLFW_KEY_S))
        camera_.Move({0.0f, 0.0f, -1.0f}, dt);
      if (inputs_.key_tokens.contains(GLFW_KEY_D))
        camera_.Move({1.0f, 0.0f, 0.0f}, dt);

      if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_SHIFT)) {
        if (inputs_.key_tokens.contains(GLFW_KEY_SPACE))
          camera_.Move({0.0f, -1.0f, 0.0f}, dt);
      } else {
        if (inputs_.key_tokens.contains(GLFW_KEY_SPACE))
          camera_.Move({0.0f, 1.0f, 0.0f}, dt);
      }

      if (inputs_.key_tokens.contains(GLFW_KEY_ESCAPE)) {
        window_.EnableMouse();
        camera_.SetMode(npr_scene::CameraMode::kOrbit);

        inputs_.curr_mouse_pos = inputs_.last_mouse_pos = window_.GetMousePos();
      }

    } else {
      camera_.Zoom(inputs_.mouse_scroll.y);
      if (inputs_.mouse_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
        camera_.Orbit(inputs_.mouse_move);
      }
      if (inputs_.mouse_buttons.contains(GLFW_MOUSE_BUTTON_RIGHT))
        camera_.Pan(inputs_.mouse_move);

      if (inputs_.key_tokens.contains(GLFW_KEY_F)) {
        window_.DisableMouse();
        camera_.SetMode(npr_scene::CameraMode::kFree);

        inputs_.curr_mouse_pos = inputs_.last_mouse_pos = window_.GetMousePos();
      }
    }

    return false;
  });
}

void App::Run() {
  while (running_) {
    timer_.Update();

    window_.PollEvents();
    inputs_.Update(window_.GetSize());
    tasks_.Process();
    camera_.Update(timer_.GetDelta());

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
