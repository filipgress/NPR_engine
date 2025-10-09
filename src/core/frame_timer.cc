#include "frame_timer.h"
using namespace std::chrono;

namespace npr_core {
void FrameTimer::Reset() {
  frame_times_.clear();
  lst = steady_clock::now();

  min_fps_ = std::numeric_limits<int>::max();
  max_fps_ = 0;
  avg_fps_ = 0;
  delta_ = 0;
  elapsed_ = 0;
}

void FrameTimer::Update() {
  auto curr = steady_clock::now();
  delta_ = duration_cast<duration<float>>(curr - lst).count();
  lst = curr;

  delta_ = delta_ == 0 ? 0.0001f : delta_;  // avoid zero division
  elapsed_ += delta_;
  int fps = 1 / delta_;

  frame_times_.push_back(fps);
  if (frame_times_.size() > kQSize_) frame_times_.pop_front();

  CalculateStats();
}
void FrameTimer::CalculateStats() {
  if (frame_times_.empty()) return;

  min_fps_ = *std::min_element(frame_times_.begin(), frame_times_.end());
  max_fps_ = *std::max_element(frame_times_.begin(), frame_times_.end());
  avg_fps_ = std::accumulate(frame_times_.begin(), frame_times_.end(), 0.0f) /
             frame_times_.size();
}
}  // namespace npr_core
