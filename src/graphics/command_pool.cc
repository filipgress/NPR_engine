#include "command_pool.h"

namespace npr_graphics {
CommandPool::CommandPool(const VulkanContext& context, uint frame_count)
    : c_{context} {
  CreateCommandPool();
  CreateCommandBuffers(frame_count);
}

CommandPool::~CommandPool() {
  if (cmd_pool_) c_.GetDevice().destroyCommandPool(cmd_pool_);
}

void CommandPool::CreateCommandPool() {
  vk::CommandPoolCreateInfo create_info{};
  create_info.flags = vk::CommandPoolCreateFlags() |
                      vk::CommandPoolCreateFlagBits::eTransient |
                      vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  create_info.queueFamilyIndex = c_.GetQFamilies().graphics_i.value();

  cmd_pool_ = c_.GetDevice().createCommandPool(create_info);
  c_.SetDbgName((uint64_t)(VkCommandPool)cmd_pool_,
                vk::ObjectType::eCommandPool, "MainCommandPool");
}

void CommandPool::CreateCommandBuffers(uint frame_count) {
  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = cmd_pool_;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = frame_count;

  cmd_buffs_ = c_.GetDevice().allocateCommandBuffers(alloc_info);
  for (size_t i = 0; i < cmd_buffs_.size(); ++i)
    c_.SetDbgName((uint64_t)(VkCommandBuffer)cmd_buffs_[i],
                  vk::ObjectType::eCommandBuffer,
                  "CmdBuff_" + std::to_string(i));
}

vk::CommandBuffer CommandPool::BeginSingleTimeCmds() const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.commandPool = cmd_pool_;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount = 1;

  vk::CommandBuffer commandBuffer;
  commandBuffer = c_.GetDevice().allocateCommandBuffers(allocInfo)[0];

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void CommandPool::EndSingleTimeCmds(vk::CommandBuffer cmd_buff) const {
  cmd_buff.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd_buff;

  auto graphics_q = c_.GetGraphicsQ();
  graphics_q.submit(submitInfo, nullptr);
  graphics_q.waitIdle();

  c_.GetDevice().freeCommandBuffers(cmd_pool_, cmd_buff);
}

}  // namespace npr_graphics
