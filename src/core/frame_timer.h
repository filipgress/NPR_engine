#ifndef FRAME_TIMER_H_
#define FRAME_TIMER_H_

namespace npr_window {
class Event;
}

namespace npr_core {
class FrameTimer {
  using EventCallbackFn = std::function<void(npr_window::Event&)>;

 public:
  FrameTimer(uint target_fps, EventCallbackFn callback_fn)
      : target_fps_{target_fps}, callback_fn_{callback_fn} {}

  uint GetMinFPS() const { return min_fps_; }
  uint GetMaxFPS() const { return max_fps_; }
  uint GetAvgFPS() const { return std::round(avg_fps_); }
  // float GetAvgFPS() const { return avg_fps_; }
  float GetDelta() const { return delta_; }
  float GetElapsed() const { return elapsed_; }

  void SetTargetFPS(uint fps) { target_fps_ = fps; }

  void Reset();
  void Wait();
  void Update();

 private:
  void CalculateStats();
  void Tick();

 private:
  std::deque<uint> frame_times_;
  static const size_t kQSize_ = 30;

  uint min_fps_{std::numeric_limits<uint>::max()};
  uint max_fps_{};
  float avg_fps_{};
  float delta_{};
  float elapsed_{};

  uint target_fps_{0};

  std::chrono::steady_clock::time_point lst_ = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lst_tick_ =
      std::chrono::steady_clock::now();

  EventCallbackFn callback_fn_;
};
}  // namespace npr_core

#endif  // FRAME_TIMER_H_
