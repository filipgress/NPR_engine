#ifndef MOUSE_EVENT_H_
#define MOUSE_EVENT_H_

#include "event.h"

namespace npr_window {
class MouseButtonEvent : public Event {
 public:
  MouseButtonEvent(int button) : button_(button) {}
  virtual ~MouseButtonEvent() = default;

  int GetButton() const { return button_; }
  virtual std::string ToString() const = 0;

 protected:
  int button_;
};

class MousePressEvent : public MouseButtonEvent {
 public:
  MousePressEvent(int button) : MouseButtonEvent(button) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseButtonPressEvent: " << button_;
    return ss.str();
  }

  EVENT_TYPE(kMousePress)
};

class MouseReleaseEvent : public MouseButtonEvent {
 public:
  MouseReleaseEvent(int button) : MouseButtonEvent(button) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseButtonReleaseEvent: " << button_;
    return ss.str();
  }

  EVENT_TYPE(kMouseRelease)
};

class MouseScrollEvent : public Event {
 public:
  MouseScrollEvent(const glm::vec2& offset) : offset_(offset) {}

  glm::vec2 GetOffset() const { return offset_; }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseScrollEvent: " << offset_.x << ", " << offset_.y;
    return ss.str();
  }

  EVENT_TYPE(kMouseScroll)

 private:
  glm::vec2 offset_;
};

class MouseMoveEvent : public Event {
 public:
  MouseMoveEvent(const glm::vec2& move) : move_(move) {}

  glm::vec2 GetMove() const { return move_; }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseMovedEvent: " << move_.x << ", " << move_.y;
    return ss.str();
  }

  EVENT_TYPE(kMouseMove)

 private:
  glm::vec2 move_;
};

class MouseEnterEvent : public Event {
 public:
  MouseEnterEvent(bool entered) : entered_(entered) {}

  bool HasEntered() const { return entered_; }

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseEnterEvent: " << (entered_ ? "Entered" : "Left");
    return ss.str();
  }

  EVENT_TYPE(kMouseEnter)

 private:
  bool entered_;
};

}  // namespace npr_window

#endif  // MOUSE_EVENT_H_
