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
      "../assets/scenes/concerto/scene.gltf");
}

void App::Run() {
  while (running_) {
    timer_.Wait();
    Update();

    if (window_.IsMinimized()) continue;
    renderer_.Render(timer_, camera_, *active_scene_,
                     loading_scene_->IsValid() || loading_scene_->IsLoading());
  }

  renderer_.WaitIdle();
}

void App::Update() {
  timer_.Update();

  // gather & process input
  inputs_.key_pressed.clear();
  window_.PollEvents();
  inputs_.Update(window_.GetSize());

  ProcessInput();

  camera_.Update(timer_.GetDelta());
  active_scene_->Update(timer_);
}

void App::ProcessInput() {
  if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
      inputs_.key_pressed.contains(GLFW_KEY_O)) {
    renderer_.GetGui().ToggleFileBrowser([this](std::string path) {
      npr_scene::SceneLoader::LoadAsync(
          renderer_, *loading_scene_, tasks_,
          [this]() {
            loading_scene_.swap(active_scene_);
            loading_scene_->Invalidate();
          },
          path);
    });
  }

  if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
      inputs_.key_pressed.contains(GLFW_KEY_R))
    renderer_.RecompileShaders();

  if (ImGui::GetIO().WantCaptureMouse) return;

  if (camera_.IsOrbit()) {
    if (inputs_.key_pressed.contains(GLFW_KEY_F)) {
      window_.DisableMouse();
      camera_.ToggleMode();
      inputs_.ResetMouse();
    }

    camera_.Zoom(inputs_.mouse_scroll.y);
    if (inputs_.mouse_buttons.contains(GLFW_MOUSE_BUTTON_LEFT))
      camera_.Orbit(inputs_.mouse_move);
    if (inputs_.mouse_buttons.contains(GLFW_MOUSE_BUTTON_RIGHT))
      camera_.Pan(inputs_.mouse_move);

  } else {
    if (inputs_.key_pressed.contains(GLFW_KEY_ESCAPE)) {
      window_.EnableMouse();
      camera_.ToggleMode();
      inputs_.ResetMouse();
    }

    float dt = timer_.GetDelta();
    glm::vec3 move_dir{0.0f};

    if (inputs_.key_tokens.contains(GLFW_KEY_W))
      move_dir += glm::vec3{0.0f, 0.0f, 1.0f};
    if (inputs_.key_tokens.contains(GLFW_KEY_A))
      move_dir += glm::vec3{-1.0f, 0.0f, 0.0f};
    if (inputs_.key_tokens.contains(GLFW_KEY_S))
      move_dir += glm::vec3{0.0f, 0.0f, -1.0f};
    if (inputs_.key_tokens.contains(GLFW_KEY_D))
      move_dir += glm::vec3{1.0f, 0.0f, 0.0f};

    if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_SHIFT) &&
        inputs_.key_tokens.contains(GLFW_KEY_SPACE))
      move_dir += glm::vec3{0.0f, -1.0f, 0.0f};

    if (!inputs_.key_tokens.contains(GLFW_KEY_LEFT_SHIFT) &&
        inputs_.key_tokens.contains(GLFW_KEY_SPACE))
      move_dir += glm::vec3{0.0f, 1.0f, 0.0f};

    float factor = 1.0f;
    if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL)) factor = .2f;

    camera_.Move(move_dir * factor, dt);
    camera_.Rotate(inputs_.mouse_move * factor);
  }
}

void App::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // app events
  dispatcher.Dispatch<AppTickEvent>([this](AppTickEvent&) {
    tasks_.Process();
    renderer_.Update();

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
