#include "input_handler.h"

#include "window/mouse_event.h"
#include "window/key_event.h"

namespace npr_core {
void InputHandler::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // mouse events
  dispatcher.Dispatch<MousePressEvent>([this](MousePressEvent& e) {
    mouse_buttons.insert(e.GetButton());
    return false;
  });
  dispatcher.Dispatch<MouseReleaseEvent>([this](MouseReleaseEvent& e) {
    mouse_buttons.erase(e.GetButton());
    return false;
  });
  dispatcher.Dispatch<MouseMoveEvent>([this](MouseMoveEvent& e) {
    curr_mouse_pos = e.GetMove();
    return false;
  });
  dispatcher.Dispatch<MouseScrollEvent>([this](MouseScrollEvent& e) {
    acc_mouse_scroll += e.GetOffset();
    return false;
  });

  // key events
  dispatcher.Dispatch<KeyPressEvent>([this](KeyPressEvent& e) {
    key_tokens[e.GetKeyCode()] = e.IsRepeat();
    return false;
  });
  dispatcher.Dispatch<KeyReleaseEvent>([this](KeyReleaseEvent& e) {
    key_tokens.erase(e.GetKeyCode());
    return false;
  });
}

void InputHandler::Update(glm::ivec2 window_size) {
  // scroll delta
  mouse_scroll = acc_mouse_scroll;
  acc_mouse_scroll = {0, 0};  // reset

  // normalize move delta
  glm::vec2 delta = curr_mouse_pos - last_mouse_pos;
  last_mouse_pos = curr_mouse_pos;

  mouse_move = {delta.x / window_size.x, delta.y / window_size.y};
}

}  // namespace npr_core
