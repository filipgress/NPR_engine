#include "descriptor_pool.h"
#include "resources.h"

namespace npr_graphics {

DescriptorPool::DescriptorPool(const VulkanContext& context,
                               const Resources& res)
    : c_{context} {
  CreateDescriptorPool();

  for (auto& ds : desc_sets_) {
    ds->AllocSets(pool_, res.GetResources().size());
    ds->WriteSets(res);
  }
};

void DescriptorPool::CreateDescriptorPool() {
  std::map<vk::DescriptorType, uint32_t> type_to_count;
  uint32_t total_sets{0};

  for (const auto& ds : desc_sets_) {
    for (const auto& size : ds->GetPoolSizes()) {
      type_to_count[size.type] += size.descriptorCount;
      total_sets += size.descriptorCount;
    }
  }

  std::vector<vk::DescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(type_to_count.size());
  for (const auto& tc : type_to_count)
    pool_sizes.push_back({tc.first, tc.second});

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = pool_sizes.size();
  poolInfo.pPoolSizes = pool_sizes.data();
  poolInfo.maxSets = total_sets;

  pool_ = c_.GetDevice().createDescriptorPool(poolInfo);
}

}  // namespace npr_graphics
