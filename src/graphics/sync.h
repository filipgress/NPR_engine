#ifndef SYNC_H_
#define SYNC_H_

#include "vulkan_context.h"

namespace npr_graphics {

struct PerFrameSync {
  vk::Fence in_flight{nullptr};
  vk::Semaphore image_available{nullptr};
};

class Sync : public npr_core::NonCopyable {
 public:
  Sync(const VulkanContext& context, uint image_count);
  ~Sync();

  PerFrameSync GetFrameSyncObjs() const { return frame_sync_objs_[frame_idx_]; }
  vk::Semaphore GetRenderFinishedSemaphore(uint image_idx) const {
    return render_finished_semaphores_[image_idx];
  }

  uint GetFrameIdx() const { return frame_idx_; }
  uint GetMaxFramesInFlight() const { return max_frames_in_flight_; }
  void Increment() { frame_idx_ = (frame_idx_ + 1) % max_frames_in_flight_; }

 private:
  const VulkanContext& c_;

  std::vector<PerFrameSync> frame_sync_objs_;
  std::vector<vk::Semaphore> render_finished_semaphores_;

  uint frame_idx_{0};
  uint max_frames_in_flight_;
};

}  // namespace npr_graphics

#endif  // SYNC_H_
