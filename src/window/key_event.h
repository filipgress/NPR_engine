#ifndef KEY_EVENT_H_
#define KEY_EVENT_H_

#include "event.h"

namespace npr_window {
class KeyEvent : public Event {
 public:
  KeyEvent(const int key_code) : key_code_(key_code) {}
  virtual ~KeyEvent() = default;

  int GetKeyCode() const { return key_code_; }
  virtual std::string ToString() const = 0;

 protected:
  int key_code_;
};

class KeyPressEvent : public KeyEvent {
 public:
  KeyPressEvent(const int key_code, const int repeat)
      : KeyEvent(key_code), repeat_(repeat) {}

  bool IsRepeat() const { return repeat_; }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "KeyPressedEvent: " << key_code_ << " (" << repeat_ << ")";

    return ss.str();
  }

  EVENT_TYPE(kKeyPress)

 private:
  bool repeat_ = false;
};

class KeyReleaseEvent : public KeyEvent {
 public:
  KeyReleaseEvent(const int key_code) : KeyEvent(key_code) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "KeyReleaseEvent: " << key_code_;

    return ss.str();
  }

  EVENT_TYPE(kKeyRelease)
};
}  // namespace npr_window

#endif  // KEY_EVENT_H_
