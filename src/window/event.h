#ifndef EVENT_H_
#define EVENT_H_

namespace npr_window {
enum class EventType {
  kAppTick,
  kWindowClose,
  kWindowResize,
  kKeyPress,
  kKeyRelease,
  kMousePress,
  kMouseRelease,
  kMouseMove,
  kMouseScroll,
  kMouseEnter
};

#define EVENT_TYPE(type)                                                      \
  static EventType GetStaticType() { return EventType::type; }                \
  virtual EventType GetEventType() const override { return GetStaticType(); } \
  virtual const char* GetName() const override { return #type; }

class Event {
  friend class EventDispatcher;

 public:
  virtual ~Event() = default;

  virtual EventType GetEventType() const = 0;
  virtual const char* GetName() const = 0;
  virtual std::string ToString() const = 0;
  bool IsHandled() const { return handled_; }

 protected:
  bool handled_{false};
};

class EventDispatcher {
 public:
  EventDispatcher(Event& event) : event_{event} {}

  template <typename T, typename F>
  bool Dispatch(const F& func) {
    if (event_.IsHandled()) return false;
    if (event_.GetEventType() == T::GetStaticType()) {
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
