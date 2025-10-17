#include "frame_timer.h"

#include "window/app_event.h"

using namespace std::chrono;

namespace npr_core {
void FrameTimer::Reset() {
  frame_times_.clear();
  lst_ = steady_clock::now();
  lst_tick_ = steady_clock::now();

  min_fps_ = std::numeric_limits<uint>::max();
  max_fps_ = 0;
  avg_fps_ = 0.0f;
  delta_ = 0.0f;
  elapsed_ = 0.0f;
}

void FrameTimer::Tick() {
  auto curr = steady_clock::now();
  if (duration_cast<milliseconds>(curr - lst_tick_).count() < 2000) return;

  lst_tick_ = curr;
  auto e = npr_window::AppTickEvent();
  callback_fn_(e);
}

void FrameTimer::Wait() {
  if (target_fps_ == 0) return;

  auto now = steady_clock::now();
  duration<float> elapsed = now - lst_;
  duration<float> desired(1.0f / target_fps_ * 0.985);

  duration<float> remaining = desired - elapsed;
  if (remaining.count() <= 0.0f) return;
  if (remaining.count() > 0.001f)
    std::this_thread::sleep_for(remaining);
  else
    while (steady_clock::now() - lst_ < desired) std::this_thread::yield();
}

void FrameTimer::Update() {
  Wait();

  auto now = steady_clock::now();
  delta_ = duration_cast<duration<float>>(now - lst_).count();
  lst_ = now;

  delta_ = delta_ == 0 ? 0.0001f : delta_;  // avoid zero division
  elapsed_ += delta_;
  uint fps = static_cast<uint>(1.0f / delta_);

  frame_times_.push_back(fps);
  if (frame_times_.size() > kQSize_) frame_times_.pop_front();

  CalculateStats();
  Tick();
}

void FrameTimer::CalculateStats() {
  if (frame_times_.empty()) return;

  min_fps_ = *std::min_element(frame_times_.begin(), frame_times_.end());
  max_fps_ = *std::max_element(frame_times_.begin(), frame_times_.end());
  avg_fps_ = std::accumulate(frame_times_.begin(), frame_times_.end(), 0.0f) /
             frame_times_.size();
}
}  // namespace npr_core
