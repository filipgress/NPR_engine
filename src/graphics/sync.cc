#include "sync.h"

namespace npr_graphics {
Sync::Sync(const VulkanContext& context, uint image_count) : c_{context} {
  auto device = c_.GetDevice();

  max_frames_in_flight_ = std::min(image_count, 3u);
  INFO("max frames(in flight): ", max_frames_in_flight_);

  frame_sync_objs_.resize(max_frames_in_flight_);
  render_finished_semaphores_.resize(image_count);

  for (uint i = 0; i < max_frames_in_flight_; i++) {
    frame_sync_objs_[i].image_available = device.createSemaphore({});
    c_.SetDbgName((uint64_t)(VkSemaphore)frame_sync_objs_[i].image_available,
                  vk::ObjectType::eSemaphore,
                  "image_available_" + std::to_string(i));

    frame_sync_objs_[i].in_flight = device.createFence(
        {vk::FenceCreateFlags() | vk::FenceCreateFlagBits::eSignaled});
    c_.SetDbgName((uint64_t)(VkFence)frame_sync_objs_[i].in_flight,
                  vk::ObjectType::eFence, "in_flight_" + std::to_string(i));
  }

  for (uint i = 0; i < image_count; i++) {
    render_finished_semaphores_[i] = device.createSemaphore({});
    c_.SetDbgName((uint64_t)(VkSemaphore)render_finished_semaphores_[i],
                  vk::ObjectType::eSemaphore,
                  "render_finished_" + std::to_string(i));
  }
}

Sync::~Sync() {
  auto device = c_.GetDevice();

  for (const auto& frame_sync : frame_sync_objs_) {
    device.destroySemaphore(frame_sync.image_available);
    device.destroyFence(frame_sync.in_flight);
  }

  for (auto semaphore : render_finished_semaphores_)
    device.destroySemaphore(semaphore);
}

}  // namespace npr_graphics
