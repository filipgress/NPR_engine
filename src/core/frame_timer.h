#ifndef FRAME_TIMER_H_
#define FRAME_TIMER_H_

namespace npr_window {
class Event;
}

namespace npr_core {
class FrameTimer {
  using EventCallbackFn = std::function<void(npr_window::Event&)>;

 public:
  FrameTimer(EventCallbackFn callback_fn) : callback_fn_{callback_fn} {}

  int GetMinFPS() const { return min_fps_; }
  int GetMaxFPS() const { return max_fps_; }
  int GetAvgFPS() const { return avg_fps_; }
  float GetDelta() const { return delta_; }
  float GetElapsed() const { return elapsed_; }

  void Reset();
  void Update();

 private:
  void CalculateStats();
  void Tick();

 private:
  std::deque<int> frame_times_;
  static const size_t kQSize_ = 30;

  int min_fps_{std::numeric_limits<int>::max()};
  int max_fps_{};
  int avg_fps_{};
  float delta_{};
  float elapsed_{};

  std::chrono::steady_clock::time_point lst_ = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lst_tick_ =
      std::chrono::steady_clock::now();

  EventCallbackFn callback_fn_;
};
}  // namespace npr_core

#endif  // FRAME_TIMER_H_
