#ifndef DESCRIPTOR_SETS_H_
#define DESCRIPTOR_SETS_H_

#include "vulkan_context.h"
#include "resources.h"

namespace npr_graphics {
class DescriptorSets : public npr_core::NonCopyable {
 public:
  DescriptorSets(const VulkanContext& context) : c_{context} {}
  virtual ~DescriptorSets() {
    if (layout_) c_.GetDevice().destroyDescriptorSetLayout(layout_);
  }

  vk::DescriptorSet GetSet(int frame_idx) const { return sets_[frame_idx]; }
  vk::DescriptorSetLayout GetLayout() const { return layout_; }

  void AllocSets(vk::DescriptorPool pool, size_t count);

  virtual void WriteSets(const Resources& res) const = 0;
  virtual std::vector<vk::DescriptorPoolSize> GetPoolSizes() const = 0;

 protected:
  virtual void CreateLayout() = 0;

 protected:
  const VulkanContext& c_;

  vk::DescriptorSetLayout layout_;
  std::vector<vk::DescriptorSet> sets_;
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_SETS_H_
