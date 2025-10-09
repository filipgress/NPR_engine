#include "app.h"

#include "window/window_event.h"
#include "window/mouse_event.h"
#include "window/key_event.h"

namespace npr_core {

void App::Run() {
  while (running_) {
    timer_.Update();

    window_.PollEvents();
    inputs_.Update(window_.GetSize());

    INFO(timer_.GetAvgFPS(), "fps");

    if (window_.IsMinimized()) continue;

    // render
  }
}

void App::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // window events
  dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
    (void)e;
    running_ = false;
    return true;
  });
  dispatcher.Dispatch<WindowResizeEvent>([](WindowResizeEvent& e) {
    (void)e;
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
    return false;
  });
  dispatcher.Dispatch<KeyReleaseEvent>([this](KeyReleaseEvent& e) {
    inputs_.key_tokens.erase(e.GetKeyCode());
    return false;
  });
}

}  // namespace npr_core
