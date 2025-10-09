#ifndef INPUT_HANDLER_H_
#define INPUT_HANDLER_H_

namespace npr_core {
struct InputHandler {
  std::unordered_map<uint16_t, bool> key_tokens;
  std::unordered_set<uint16_t> mouse_buttons;

  glm::vec2 curr_mouse_pos{};
  glm::vec2 last_mouse_pos{};

  glm::vec2 acc_mouse_scroll{};

  glm::vec2 mouse_move{};
  glm::vec2 mouse_scroll{};

  void Update(glm::ivec2 window_size) {
    // scroll delta
    mouse_scroll = acc_mouse_scroll;
    acc_mouse_scroll = {0, 0};  // reset

    // normalize move delta
    glm::vec2 delta = curr_mouse_pos - last_mouse_pos;
    last_mouse_pos = curr_mouse_pos;

    mouse_move = {delta.x / window_size.x, delta.y / window_size.y};
  }
};
}  // namespace npr_core

#endif  // INPUT_HANDLER_H_
