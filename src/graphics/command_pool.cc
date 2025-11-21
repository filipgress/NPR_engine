#include "command_pool.h"

namespace npr_graphics {

CommandPool::CommandPool(const Context& ctx, uint count,
                         vk::CommandPoolCreateFlags usage, uint32_t queue_idx,
                         const std::string& dbg_name)
    : ctx_{ctx}, dbg_name_{dbg_name} {
  CreateCommandPool(usage, queue_idx);
  CreateCommandBuffers(count);
}

CommandPool::~CommandPool() {
  ctx_.GetDevice().waitIdle();
  if (cmd_pool_) ctx_.GetDevice().destroyCommandPool(cmd_pool_);
}

void CommandPool::CreateCommandPool(vk::CommandPoolCreateFlags usage,
                                    uint32_t queue_idx) {
  vk::CommandPoolCreateInfo create_info{};
  create_info.flags = usage;
  create_info.queueFamilyIndex = queue_idx;

  cmd_pool_ = ctx_.GetDevice().createCommandPool(create_info);
  ctx_.SetDbgName((uint64_t)(VkCommandPool)cmd_pool_,
                  vk::ObjectType::eCommandPool, dbg_name_);
}

void CommandPool::CreateCommandBuffers(uint count) {
  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = cmd_pool_;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = count;

  cmd_buffs_ = ctx_.GetDevice().allocateCommandBuffers(alloc_info);
  for (size_t i = 0; i < cmd_buffs_.size(); ++i)
    ctx_.SetDbgName((uint64_t)(VkCommandBuffer)cmd_buffs_[i],
                    vk::ObjectType::eCommandBuffer,
                    dbg_name_ + "_cmd_buff_" + std::to_string(i));
}

vk::CommandBuffer CommandPool::BeginSingleTimeCmds() const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.commandPool = cmd_pool_;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount = 1;

  vk::CommandBuffer cmd_buff;
  cmd_buff = ctx_.GetDevice().allocateCommandBuffers(allocInfo)[0];

  vk::CommandBufferBeginInfo begin_info{};
  begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  cmd_buff.begin(begin_info);

  return cmd_buff;
}

void CommandPool::EndSingleTimeCmds(vk::CommandBuffer cmd_buff) const {
  cmd_buff.end();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buff;

  auto graphics_q = ctx_.GetGraphicsQ();
  graphics_q.submit(submit_info, nullptr);
  graphics_q.waitIdle();

  ctx_.GetDevice().freeCommandBuffers(cmd_pool_, cmd_buff);
}

}  // namespace npr_graphics
