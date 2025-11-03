#ifndef INPUT_HANDLER_H_
#define INPUT_HANDLER_H_

#include "window/event.h"

namespace npr_core {
struct InputHandler {
  std::unordered_map<uint16_t, bool> key_tokens;
  std::unordered_set<uint16_t> mouse_buttons;

  glm::vec2 curr_mouse_pos{};
  glm::vec2 last_mouse_pos{};

  glm::vec2 acc_mouse_scroll{};

  glm::vec2 mouse_move{};
  glm::vec2 mouse_scroll{};

  void OnEvent(npr_window::Event& e);
  void Update(glm::ivec2 window_size);
};
}  // namespace npr_core

#endif  // INPUT_HANDLER_H_
