#ifndef APP_EVENT_H_
#define APP_EVENT_H_

#include "event.h"

namespace npr_window {
class AppTickEvent : public Event {
 public:
  AppTickEvent() = default;

  EVENT_TYPE(kAppTick)
  std::string ToString() const override { return GetName(); }
};

class WindowResizeEvent : public Event {
 public:
  WindowResizeEvent(const glm::ivec2& size) : size_{size} {}

  glm::ivec2 GetSize() { return size_; }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "WindowResizeEvent: " << size_.x << ", " << size_.y;
    return ss.str();
  }

  EVENT_TYPE(kWindowResize)

 private:
  glm::ivec2 size_;
};

class WindowCloseEvent : public Event {
 public:
  std::string ToString() const override { return "WindowCloseEvent"; }

  EVENT_TYPE(kWindowClose)
};
}  // namespace npr_window

#endif  // APP_EVENT_H_
