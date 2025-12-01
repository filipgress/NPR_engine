#include "descriptor_pool.h"
#include "resources.h"

namespace npr_graphics {

DescriptorPool::DescriptorPool(const Context& ctx, const Resources& resrc)
    : ctx_{ctx},
      gbuff_sets_{ctx, resrc.GetFrameCount()},
      camera_sets_{ctx, resrc.GetFrameCount()},
      material_sets_{ctx, resrc.GetFrameCount()},

      dir_light_sets_{ctx, resrc.GetFrameCount()},
      point_light_sets_{ctx, resrc.GetFrameCount()},
      spot_light_sets_{ctx, resrc.GetFrameCount()},

      ao_set_{ctx},
      ao_res_sets_{ctx, resrc.GetFrameCount()},
      ao_temp_sets_{ctx, resrc.GetFrameCount()},

      abuff_sets_{ctx, resrc.GetFrameCount()},
      wboit_input_sets_{ctx, resrc.GetFrameCount()},

      bright_sets_{ctx, resrc.GetFrameCount()},
      bright_temp_sets_{ctx, resrc.GetFrameCount()},

      depth_sets_{ctx, resrc.GetFrameCount()},
      coc_sets_{ctx, resrc.GetFrameCount()},
      dof_set_{ctx},

      color_sets_{ctx, resrc.GetFrameCount()},
      present_sets_{ctx, resrc.GetFrameCount()} {
  CreateDescriptorPool();

  gbuff_sets_.AllocSets(pool_);
  gbuff_sets_.Update(resrc);

  camera_sets_.AllocSets(pool_);
  camera_sets_.Update(resrc);

  material_sets_.AllocSets(pool_);
  material_sets_.Update(resrc);

  dir_light_sets_.AllocSets(pool_);
  dir_light_sets_.Update(resrc);
  point_light_sets_.AllocSets(pool_);
  point_light_sets_.Update(resrc);
  spot_light_sets_.AllocSets(pool_);
  spot_light_sets_.Update(resrc);

  // ao
  ao_set_.AllocSets(pool_);
  ao_set_.Update(resrc);
  ao_res_sets_.AllocSets(pool_);
  ao_res_sets_.Update(resrc);
  ao_temp_sets_.AllocSets(pool_);
  ao_temp_sets_.Update(resrc);

  // abuff & wboit
  abuff_sets_.AllocSets(pool_);
  abuff_sets_.Update(resrc);
  wboit_input_sets_.AllocSets(pool_);
  wboit_input_sets_.Update(resrc);

  // bloom
  bright_sets_.AllocSets(pool_);
  bright_sets_.Update(resrc);
  bright_temp_sets_.AllocSets(pool_);
  bright_temp_sets_.Update(resrc);

  // dof
  depth_sets_.AllocSets(pool_);
  depth_sets_.Update(resrc);
  coc_sets_.AllocSets(pool_);
  coc_sets_.Update(resrc);
  dof_set_.AllocSets(pool_);
  dof_set_.Update(resrc);

  // color
  color_sets_.AllocSets(pool_);
  color_sets_.Update(resrc);
  present_sets_.AllocSets(pool_);
  present_sets_.Update(resrc);
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

  const auto& light_sizes = dir_light_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), light_sizes.begin(), light_sizes.end());

  const auto& point_light_sizes = point_light_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), point_light_sizes.begin(),
                    point_light_sizes.end());

  const auto& spot_light_sizes = spot_light_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), spot_light_sizes.begin(),
                    spot_light_sizes.end());

  // ao
  const auto& ao_sizes = ao_set_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), ao_sizes.begin(), ao_sizes.end());
  const auto& ao_res_sizes = ao_res_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), ao_res_sizes.begin(), ao_res_sizes.end());
  const auto& ao_temp_sizes = ao_temp_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), ao_temp_sizes.begin(),
                    ao_temp_sizes.end());

  // abuff & wboit
  const auto& abuff_sizes = abuff_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), abuff_sizes.begin(), abuff_sizes.end());
  const auto& wboit_sizes = wboit_input_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), wboit_sizes.begin(), wboit_sizes.end());

  // bloom
  const auto& bright_sizes = bright_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), bright_sizes.begin(), bright_sizes.end());
  const auto& bright_temp_sizes = bright_temp_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), bright_temp_sizes.begin(),
                    bright_temp_sizes.end());

  // dof
  const auto& depth_sizes = depth_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), depth_sizes.begin(), depth_sizes.end());
  const auto& coc_sizes = coc_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), coc_sizes.begin(), coc_sizes.end());
  const auto& dof_sizes = dof_set_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), dof_sizes.begin(), dof_sizes.end());

  // color
  const auto& color_sizes = color_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), color_sizes.begin(), color_sizes.end());
  const auto& present_sizes = present_sets_.GetPoolSizes();
  pool_sizes.insert(pool_sizes.end(), present_sizes.begin(),
                    present_sizes.end());

  uint32_t max_sets =
      gbuff_sets_.GetCount() + camera_sets_.GetCount() +
      material_sets_.GetCount() + dir_light_sets_.GetCount() +
      point_light_sets_.GetCount() + spot_light_sets_.GetCount() +
      ao_set_.GetCount() + ao_res_sets_.GetCount() + ao_temp_sets_.GetCount() +
      abuff_sets_.GetCount() + wboit_input_sets_.GetCount() +
      bright_sets_.GetCount() + bright_temp_sets_.GetCount() +
      depth_sets_.GetCount() + dof_set_.GetCount() + coc_sets_.GetCount() +
      color_sets_.GetCount() + present_sets_.GetCount();

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = pool_sizes.size();
  poolInfo.pPoolSizes = pool_sizes.data();
  poolInfo.maxSets = max_sets;

  pool_ = ctx_.GetDevice().createDescriptorPool(poolInfo);
  ctx_.SetDbgName((uint64_t)(VkDescriptorPool)pool_,
                  vk::ObjectType::eDescriptorPool, "main_descriptor_pool");
}

void DescriptorPool::UpdateDescriptors(const Resources& resrc) {
  gbuff_sets_.Update(resrc);

  ao_res_sets_.Update(resrc);
  ao_temp_sets_.Update(resrc);

  wboit_input_sets_.Update(resrc);

  bright_sets_.Update(resrc);
  bright_temp_sets_.Update(resrc);

  depth_sets_.Update(resrc);
  coc_sets_.Update(resrc);

  color_sets_.Update(resrc);
  present_sets_.Update(resrc);
}

/*
 * TexDescriptorPool
 */
TexDescriptorPool::TexDescriptorPool(const Context& ctx,
                                     const std::string& dbg_name)
    : ctx_{ctx}, tex_set_{ctx} {
  const auto& tex_sizes = tex_set_.GetPoolSizes();

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = tex_sizes.size();
  poolInfo.pPoolSizes = tex_sizes.data();
  poolInfo.maxSets = tex_set_.GetCount();

  pool_ = ctx_.GetDevice().createDescriptorPool(poolInfo);
  ctx_.SetDbgName((uint64_t)(VkDescriptorPool)pool_,
                  vk::ObjectType::eDescriptorPool, "tex_desc_pool_" + dbg_name);

  tex_set_.AllocSets(pool_);
}

/*
 * ImGuiDescriptorPool
 */
ImGuiDescriptorPool::ImGuiDescriptorPool(const Context& ctx) : ctx_{ctx} {
  std::vector<vk::DescriptorPoolSize> pool_sizes = {
      {vk::DescriptorType::eSampler, 1000},
      {vk::DescriptorType::eCombinedImageSampler, 1000},
      {vk::DescriptorType::eSampledImage, 1000},
      {vk::DescriptorType::eStorageImage, 1000},
      {vk::DescriptorType::eUniformTexelBuffer, 1000},
      {vk::DescriptorType::eStorageTexelBuffer, 1000},
      {vk::DescriptorType::eUniformBuffer, 1000},
      {vk::DescriptorType::eStorageBuffer, 1000},
      {vk::DescriptorType::eUniformBufferDynamic, 1000},
      {vk::DescriptorType::eStorageBufferDynamic, 1000},
      {vk::DescriptorType::eInputAttachment, 1000}};

  vk::DescriptorPoolCreateInfo pool_info{};
  pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  pool_info.maxSets = 1000 * pool_sizes.size();
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();

  pool_ = ctx_.GetDevice().createDescriptorPool(pool_info);
  ctx_.SetDbgName((uint64_t)(VkDescriptorPool)pool_,
                  vk::ObjectType::eDescriptorPool, "gui_desc_pool");
}

}  // namespace npr_graphics
