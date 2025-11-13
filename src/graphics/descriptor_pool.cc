#include "descriptor_pool.h"
#include "resources.h"

namespace npr_graphics {

DescriptorPool::DescriptorPool(const VulkanContext& context,
                               const Resources& res)
    : c_{context},
      gbuff_sets_{context, res.GetFrameCount()},
      camera_sets_{context, res.GetFrameCount()},
      material_sets_{context, res.GetFrameCount()},
      abuff_sets_{context, res.GetFrameCount()},
      wboit_input_sets_{context, res.GetFrameCount()},
      present_sets_{context, res.GetFrameCount()} {
  CreateDescriptorPool();

  gbuff_sets_.AllocSets(pool_);
  gbuff_sets_.Update(res);

  camera_sets_.AllocSets(pool_);
  camera_sets_.Update(res);

  material_sets_.AllocSets(pool_);
  material_sets_.Update(res);

  abuff_sets_.AllocSets(pool_);
  abuff_sets_.Update(res);

  wboit_input_sets_.AllocSets(pool_);
  wboit_input_sets_.Update(res);

  present_sets_.AllocSets(pool_);
  present_sets_.Update(res);
};

void DescriptorPool::CreateDescriptorPool() {
  std::vector<vk::DescriptorPoolSize> pool_sizes;

  const auto& gbuff_sizes = gbuff_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), gbuff_sizes.begin(), gbuff_sizes.end());

  const auto& camera_sizes = camera_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), camera_sizes.begin(), camera_sizes.end());

  const auto& material_sizes = material_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), material_sizes.begin(),
                    material_sizes.end());

  const auto& abuff_sizes = abuff_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), abuff_sizes.begin(), abuff_sizes.end());

  const auto& wboit_sizes = wboit_input_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), wboit_sizes.begin(), wboit_sizes.end());

  const auto& present_sizes = present_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), present_sizes.begin(),
                    present_sizes.end());

  uint32_t max_sets = gbuff_sets_.GetCount() + camera_sets_.GetCount() +
                      material_sets_.GetCount() + abuff_sets_.GetCount() +
                      wboit_input_sets_.GetCount() + present_sets_.GetCount();

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = pool_sizes.size();
  poolInfo.pPoolSizes = pool_sizes.data();
  poolInfo.maxSets = max_sets;

  pool_ = c_.GetDevice().createDescriptorPool(poolInfo);
  c_.SetDbgName((uint64_t)(VkDescriptorPool)pool_,
                vk::ObjectType::eDescriptorPool, "main_descriptor_pool");
}

/*
 * TexDescriptorPool
 */
TexDescriptorPool::TexDescriptorPool(const VulkanContext& context)
    : c_{context}, tex_set_{context} {
  CreateDescriptorPool();
  tex_set_.AllocSets(pool_);
}

void TexDescriptorPool::CreateDescriptorPool() {
  const auto& tex_sizes = tex_set_.GetPoolSizes();

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = tex_sizes.size();
  poolInfo.pPoolSizes = tex_sizes.data();
  poolInfo.maxSets = tex_set_.GetCount();

  pool_ = c_.GetDevice().createDescriptorPool(poolInfo);
  c_.SetDbgName((uint64_t)(VkDescriptorPool)pool_,
                vk::ObjectType::eDescriptorPool, "tex_descriptor_pool");
}

}  // namespace npr_graphics
