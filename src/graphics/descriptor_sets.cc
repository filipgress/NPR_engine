#include "descriptor_sets.h"
#include "resources.h"

namespace npr_graphics {

void DescriptorSets::AllocSets(vk::DescriptorPool pool, size_t count) {
  std::vector<vk::DescriptorSetLayout> layouts(count, layout_);

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = count;
  allocInfo.pSetLayouts = layouts.data();

  sets_ = c_.GetDevice().allocateDescriptorSets(allocInfo);
}

}  // namespace npr_graphics
