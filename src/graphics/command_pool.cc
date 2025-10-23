#include "command_pool.h"

namespace npr_graphics {

CommandPool::CommandPool(const VulkanContext& context, uint count,
                         vk::CommandPoolCreateFlags usage, uint32_t queue_idx,
                         const std::string& dbg_name)
    : c_{context}, dbg_name_{dbg_name} {
  CreateCommandPool(usage, queue_idx);
  CreateCommandBuffers(count);
}

CommandPool::~CommandPool() {
  c_.GetDevice().waitIdle();
  if (cmd_pool_) c_.GetDevice().destroyCommandPool(cmd_pool_);
}

void CommandPool::CreateCommandPool(vk::CommandPoolCreateFlags usage,
                                    uint32_t queue_idx) {
  vk::CommandPoolCreateInfo create_info{};
  create_info.flags = usage;
  create_info.queueFamilyIndex = queue_idx;

  cmd_pool_ = c_.GetDevice().createCommandPool(create_info);
  c_.SetDbgName((uint64_t)(VkCommandPool)cmd_pool_,
                vk::ObjectType::eCommandPool, dbg_name_);
}

void CommandPool::CreateCommandBuffers(uint count) {
  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = cmd_pool_;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = count;

  cmd_buffs_ = c_.GetDevice().allocateCommandBuffers(alloc_info);
  for (size_t i = 0; i < cmd_buffs_.size(); ++i)
    c_.SetDbgName((uint64_t)(VkCommandBuffer)cmd_buffs_[i],
                  vk::ObjectType::eCommandBuffer,
                  dbg_name_ + "_cmd_buff_" + std::to_string(i));
}

}  // namespace npr_graphics
