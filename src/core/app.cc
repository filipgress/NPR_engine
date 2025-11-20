#include "app.h"

#include "window/app_event.h"
#include "scene/scene_loader.h"

namespace npr_core {

App::App()
    : active_scene_{std::make_unique<npr_scene::Scene>()},
      loading_scene_{std::make_unique<npr_scene::Scene>()} {
  npr_scene::SceneLoader::LoadAsync(
      renderer_, *loading_scene_, tasks_,
      [this]() { loading_scene_.swap(active_scene_); },
      "../assets/ds/scene.gltf");
}

void App::Run() {
  while (running_) {
    Update();

    if (window_.IsMinimized()) continue;
    renderer_.Render(camera_, *active_scene_, loading_scene_->IsLoading(),
                     timer_.GetElapsed());
  }

  renderer_.Finish();
}

void App::Update() {
  timer_.Update();

  // gather inputs and process tasks
  window_.PollEvents();
  inputs_.Update(window_.GetSize());
  tasks_.Process();

  ProcessInput();

  camera_.Update(timer_.GetDelta());
  active_scene_->Update();
}

void App::ProcessInput() {
  if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
      inputs_.key_tokens.contains(GLFW_KEY_R))
    renderer_.RecompileShaders();

  if (inputs_.key_tokens.contains(GLFW_KEY_I)) INFO(timer_.GetAvgFPS(), "fps");

  if (inputs_.key_tokens.contains(GLFW_KEY_O)) {
    if (active_scene_->IsValid()) {
      flecs::entity first_camera = active_scene_->GetCameraQuery().first();
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
      camera_.ToggleMode();
      inputs_.ResetMouse();
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
      camera_.ToggleMode();
      inputs_.ResetMouse();
    }
  }
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
