#include "sync.h"

namespace npr_graphics {
Sync::Sync(const Context& ctx, uint image_count) : ctx_{ctx} {
  auto device = ctx_.GetDevice();

  frame_count_ = std::min(image_count, 3u);
  INFO("max frames(in flight): ", frame_count_);

  frame_sync_objs_.resize(frame_count_);
  render_finished_semaphores_.resize(image_count);

  for (uint i = 0; i < frame_count_; i++) {
    frame_sync_objs_[i].image_available = device.createSemaphore({});
    ctx_.SetDbgName((uint64_t)(VkSemaphore)frame_sync_objs_[i].image_available,
                    vk::ObjectType::eSemaphore,
                    "image_available_" + std::to_string(i));

    frame_sync_objs_[i].in_flight = device.createFence(
        {vk::FenceCreateFlags() | vk::FenceCreateFlagBits::eSignaled});
    ctx_.SetDbgName((uint64_t)(VkFence)frame_sync_objs_[i].in_flight,
                    vk::ObjectType::eFence, "in_flight_" + std::to_string(i));
  }

  for (uint i = 0; i < image_count; i++) {
    render_finished_semaphores_[i] = device.createSemaphore({});
    ctx_.SetDbgName((uint64_t)(VkSemaphore)render_finished_semaphores_[i],
                    vk::ObjectType::eSemaphore,
                    "render_finished_" + std::to_string(i));
  }
}

Sync::~Sync() {
  auto device = ctx_.GetDevice();

  for (const auto& frame_sync : frame_sync_objs_) {
    device.destroySemaphore(frame_sync.image_available);
    device.destroyFence(frame_sync.in_flight);
  }

  for (auto semaphore : render_finished_semaphores_)
    device.destroySemaphore(semaphore);
}

}  // namespace npr_graphics
