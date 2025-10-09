#ifndef WINDOW_EVENT_H_
#define WINDOW_EVENT_H_

#include "event.h"

namespace npr_window {
class WindowResizeEvent : public Event {
 public:
  WindowResizeEvent(const glm::ivec2& size) : size_{size} {}

  glm::ivec2 GetSize() { return size_; }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "WindowResizeEvent: " << size_.x << ", " << size_.y;
    return ss.str();
  }

 private:
  glm::ivec2 size_;
};

class WindowCloseEvent : public Event {
 public:
  std::string ToString() const override { return "WindowCloseEvent"; }
};
}  // namespace npr_window

#endif  // WINDOW_EVENT_H_
