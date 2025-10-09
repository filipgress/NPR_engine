#ifndef EVENT_H_
#define EVENT_H_

namespace npr_window {
class Event {
  friend class EventDispatcher;

 public:
  virtual ~Event() = default;

  virtual std::string ToString() const = 0;
  bool isHandled() const { return handled_; }

 protected:
  bool handled_{false};
};

class EventDispatcher {
 public:
  EventDispatcher(Event& event) : event_{event} {}

  template <typename T, typename F>
  bool Dispatch(const F& func) {
    if (dynamic_cast<T*>(&event_)) {
      event_.handled_ |= func(static_cast<T&>(event_));
      return true;
    }
    return false;
  }

 private:
  Event& event_;
};
}  // namespace npr_window

#endif  // EVENT_H_
