#ifndef SYNC_H_
#define SYNC_H_

#include "context.h"

namespace npr_graphics {

struct PerFrameSync {
  vk::Fence in_flight{nullptr};
  vk::Semaphore image_available{nullptr};
};

class Sync : public npr_core::NonCopyable {
 public:
  Sync(const Context& ctx, uint image_count);
  ~Sync();

  PerFrameSync GetFrameSyncObjs() const { return frame_sync_objs_[frame_idx_]; }
  vk::Semaphore GetRenderFinished(uint image_idx) const {
    return render_finished_semaphores_[image_idx];
  }

  uint GetFrameIdx() const { return frame_idx_; }
  uint GetFrameCount() const { return frame_count_; }

  void Increment() { frame_idx_ = (frame_idx_ + 1) % frame_count_; }

 private:
  const Context& ctx_;

  std::vector<PerFrameSync> frame_sync_objs_;
  std::vector<vk::Semaphore> render_finished_semaphores_;

  uint frame_idx_{0};
  uint frame_count_;  // max frames in flight
};

}  // namespace npr_graphics

#endif  // SYNC_H_
