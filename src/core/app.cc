#include "app.h"

#include "window/app_event.h"
#include "window/mouse_event.h"
#include "window/key_event.h"

#include "scene/scene_loader.h"

namespace npr_core {

void App::Run() {
  npr_scene::Scene scene;
  bool load = true;
  npr_scene::SceneLoader::LoadAsync(renderer_.GetContext(), scene,
                                    "../assets/market/scene.gltf");

  while (running_) {
    timer_.Update();

    window_.PollEvents();
    inputs_.Update(window_.GetSize());

    if (scene.IsValid() && load) {
      load = false;
      auto cmd_buff = scene.GetGpuResources().cmd_pool->GetCmdBuff();

      vk::SubmitInfo submit_info{};
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers = &cmd_buff;

      INFO("SUBMIT");
      renderer_.GetContext().GetGraphicsQ().submit(submit_info);
    }

    if (window_.IsMinimized()) continue;
    renderer_.Render();
  }

  renderer_.GetContext().GetDevice().waitIdle();
}

void App::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // app events
  dispatcher.Dispatch<AppTickEvent>([this](AppTickEvent& /*e*/) {
    renderer_.SwapShaders();
    return true;
  });

  // window events
  dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& /*e*/) {
    running_ = false;
    return true;
  });
  dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& /*e*/) {
    renderer_.OnWindowResize();
    return true;
  });

  // mouse events
  dispatcher.Dispatch<MousePressEvent>([this](MousePressEvent& e) {
    inputs_.mouse_buttons.insert(e.GetButton());
    return false;
  });
  dispatcher.Dispatch<MouseReleaseEvent>([this](MouseReleaseEvent& e) {
    inputs_.mouse_buttons.erase(e.GetButton());
    return false;
  });
  dispatcher.Dispatch<MouseMoveEvent>([this](MouseMoveEvent& e) {
    inputs_.curr_mouse_pos = e.GetMove();
    return false;
  });
  dispatcher.Dispatch<MouseScrollEvent>([this](MouseScrollEvent& e) {
    inputs_.acc_mouse_scroll += e.GetOffset();
    return false;
  });

  // key events
  dispatcher.Dispatch<KeyPressEvent>([this](KeyPressEvent& e) {
    inputs_.key_tokens[e.GetKeyCode()] = e.IsRepeat();

    if (inputs_.key_tokens.contains(GLFW_KEY_LEFT_CONTROL) &&
        inputs_.key_tokens.contains(GLFW_KEY_R)) {
      renderer_.RecompileShaders();
      return true;
    }
    if (inputs_.key_tokens.contains(GLFW_KEY_I)) {
      INFO(timer_.GetAvgFPS(), "fps");
      return true;
    }

    return false;
  });
  dispatcher.Dispatch<KeyReleaseEvent>([this](KeyReleaseEvent& e) {
    inputs_.key_tokens.erase(e.GetKeyCode());
    return false;
  });
}

}  // namespace npr_core
