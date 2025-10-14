#ifndef COMMAND_POOL_H_
#define COMMAND_POOL_H_

#include "vulkan_context.h"

namespace npr_graphics {
class CommandPool : public npr_core::NonCopyable {
 public:
  CommandPool(const VulkanContext& context, uint max_frames_in_flight);
  ~CommandPool();

  vk::CommandBuffer GetCmdBuff(uint frame_idx) const {
    return cmd_buffs_[frame_idx];
  }

  vk::CommandBuffer BeginSingleTimeCmds() const;
  void EndSingleTimeCmds(vk::CommandBuffer cmd_buff) const;

 private:
  void CreateCommandPool();
  void CreateCommandBuffers(uint max_frames_in_flight);

 private:
  const VulkanContext& c_;

  vk::CommandPool cmd_pool_{nullptr};
  std::vector<vk::CommandBuffer> cmd_buffs_;
};
}  // namespace npr_graphics

#endif  // COMMAND_POOL_H_
