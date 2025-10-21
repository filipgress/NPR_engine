#ifndef DESCRIPTOR_POOL_H_
#define DESCRIPTOR_POOL_H_

#include "vulkan_context.h"
#include "descriptor_sets.h"
#include "resources.h"

namespace npr_graphics {

class DescriptorPool : public npr_core::NonCopyable {
 public:
  DescriptorPool(const VulkanContext& context, const Resources& res);
  ~DescriptorPool() {
    if (pool_) c_.GetDevice().destroyDescriptorPool(pool_);
  }

 private:
  void CreateDescriptorPool();

 private:
  const VulkanContext& c_;

  vk::DescriptorPool pool_{nullptr};
  std::array<std::unique_ptr<DescriptorSets>, 0> desc_sets_{
      // std::make_unique<GBuffSets>(c_)
  };
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_POOL_H_
