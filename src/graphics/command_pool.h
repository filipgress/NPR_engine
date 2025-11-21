#ifndef COMMAND_POOL_H_
#define COMMAND_POOL_H_

#include "context.h"

namespace npr_graphics {
class CommandPool : public npr_core::NonCopyable {
 public:
  CommandPool(const Context& ctx, uint count, vk::CommandPoolCreateFlags usage,
              uint32_t queue_idx, const std::string& dbg_name);
  ~CommandPool();

  vk::CommandBuffer GetCmdBuff(uint idx = 0) const { return cmd_buffs_[idx]; }

  vk::CommandBuffer BeginSingleTimeCmds() const;
  void EndSingleTimeCmds(vk::CommandBuffer cmd_buff) const;

 private:
  void CreateCommandPool(vk::CommandPoolCreateFlags usage, uint32_t queue_idx);
  void CreateCommandBuffers(uint count);

 private:
  const Context& ctx_;

  vk::CommandPool cmd_pool_{nullptr};
  std::vector<vk::CommandBuffer> cmd_buffs_;

  std::string dbg_name_;
};

class GraphicsCommandPool : public CommandPool {
 public:
  GraphicsCommandPool(const Context& ctx, uint frame_count)
      : CommandPool{ctx,
                    frame_count,
                    {vk::CommandPoolCreateFlagBits::eTransient |
                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer},
                    ctx.GetQFamilies().graphics_i.value(),
                    "graphics_cmd_pool"} {}
};

}  // namespace npr_graphics

#endif  // COMMAND_POOL_H_
