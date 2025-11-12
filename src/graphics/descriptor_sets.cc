#include "descriptor_sets.h"
#include "resources.h"

namespace npr_graphics {

void BaseDescSets::AllocSets(vk::DescriptorPool pool) {
  std::vector<vk::DescriptorSetLayout> layouts(count_, layout_);

  vk::DescriptorSetAllocateInfo alloc_info{};
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = count_;
  alloc_info.pSetLayouts = layouts.data();

  sets_ = c_.GetDevice().allocateDescriptorSets(alloc_info);
}

/*
 * SingleBuffSets
 */
void SingleBuffSets::CreateLayout() {
  vk::DescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = desc_type_;
  binding.descriptorCount = 1;
  binding.stageFlags = stage_flags_;

  vk::DescriptorSetLayoutCreateInfo layout_info{};
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;

  layout_ = c_.GetDevice().createDescriptorSetLayout(layout_info);
  c_.SetDbgName((uint64_t)(VkDescriptorSetLayout)layout_,
                vk::ObjectType::eDescriptorSetLayout, "buffer_set_layout");
}

void SingleBuffSets::Update(vk::Buffer buffer, vk::DeviceSize range,
                            uint idx) const {
  vk::DescriptorBufferInfo buffer_info{};
  buffer_info.buffer = buffer;
  buffer_info.offset = 0;
  buffer_info.range = range;

  vk::WriteDescriptorSet desc_write{};
  desc_write.dstSet = sets_[idx];
  desc_write.dstBinding = 0;
  desc_write.dstArrayElement = 0;
  desc_write.descriptorType = desc_type_;
  desc_write.descriptorCount = 1;
  desc_write.pBufferInfo = &buffer_info;

  c_.GetDevice().updateDescriptorSets(1, &desc_write, 0, nullptr);
}

/*
 * SingleTexSets
 */
void SingleTexSets::CreateLayout() {
  vk::DescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  binding.descriptorCount = 1;
  binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

  vk::DescriptorSetLayoutCreateInfo layout_info{};
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;

  layout_ = c_.GetDevice().createDescriptorSetLayout(layout_info);
  c_.SetDbgName((uint64_t)(VkDescriptorSetLayout)layout_,
                vk::ObjectType::eDescriptorSetLayout, "texture_set_layout");
}

void SingleTexSets::Update(const Resources& res) const {
  for (size_t i = 0; i < res.GetResources().size(); i++) {
    auto* tex = GetAttach(res, i);

    vk::DescriptorImageInfo image_info{};
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_info.imageView = tex->GetImageView();
    image_info.sampler = tex->GetSampler();

    vk::WriteDescriptorSet desc_write{};

    desc_write.dstSet = sets_[i];
    desc_write.dstBinding = 0;
    desc_write.dstArrayElement = 0;
    desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    desc_write.descriptorCount = 1;
    desc_write.pImageInfo = &image_info;

    c_.GetDevice().updateDescriptorSets(1, &desc_write, 0, nullptr);
  }
}

void CameraUnifSets::Update(const Resources& res) const {
  const auto& per_frame_res = res.GetResources();
  assert(count_ == per_frame_res.size());

  for (size_t i = 0; i < per_frame_res.size(); i++)
    SingleBuffSets::Update(per_frame_res[i].camera_ubo->GetBuffer(),
                           VK_WHOLE_SIZE, i);
}

void MaterialUnifSets::Update(const Resources& res) const {
  const auto& per_frame_res = res.GetResources();
  assert(count_ == per_frame_res.size());

  for (size_t i = 0; i < per_frame_res.size(); i++)
    SingleBuffSets::Update(per_frame_res[i].material_ubo->GetBuffer(),
                           per_frame_res[i].material_ubo->GetElemSize(), i);
}

/*
 * GBuffSets
 */
void GBuffSets::CreateLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 5> bindings;

  // albedo
  bindings[0].binding = 0;
  bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // emissive
  bindings[1].binding = 1;
  bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // position
  bindings[2].binding = 2;
  bindings[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // normal
  bindings[3].binding = 3;
  bindings[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  bindings[3].descriptorCount = 1;
  bindings[3].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // coverage
  bindings[4].binding = 4;
  bindings[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  bindings[4].descriptorCount = 1;
  bindings[4].stageFlags = vk::ShaderStageFlagBits::eFragment;

  vk::DescriptorSetLayoutCreateInfo layout_info{};
  layout_info.bindingCount = bindings.size();
  layout_info.pBindings = bindings.data();

  layout_ = c_.GetDevice().createDescriptorSetLayout(layout_info);
  c_.SetDbgName((uint64_t)(VkDescriptorSetLayout)layout_,
                vk::ObjectType::eDescriptorSetLayout, "gbuff_set_layout");
}

void GBuffSets::Update(const Resources& res) const {
  const auto& per_frame_res = res.GetResources();
  assert(count_ == per_frame_res.size());

  for (size_t i = 0; i < per_frame_res.size(); ++i) {
    const auto& frame_res = per_frame_res[i];
    std::array<vk::DescriptorImageInfo, 5> image_infos;

    // albedo
    image_infos[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[0].imageView = frame_res.albedo_metallic_ms->GetImageView();
    image_infos[0].sampler = frame_res.albedo_metallic_ms->GetSampler();

    // emissive
    image_infos[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[1].imageView = frame_res.emissive_roughness_ms->GetImageView();
    image_infos[1].sampler = frame_res.emissive_roughness_ms->GetSampler();

    // position
    image_infos[2].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[2].imageView = frame_res.position_ms->GetImageView();
    image_infos[2].sampler = frame_res.position_ms->GetSampler();

    // normal
    image_infos[3].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[3].imageView = frame_res.normal_ms->GetImageView();
    image_infos[3].sampler = frame_res.normal_ms->GetSampler();

    // coverage
    image_infos[4].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[4].imageView = frame_res.coverage_res->GetImageView();
    image_infos[4].sampler = frame_res.coverage_res->GetSampler();

    std::array<vk::WriteDescriptorSet, 5> desc_writes;
    for (size_t j = 0; j < 5; ++j) {
      desc_writes[j].dstSet = sets_[i];
      desc_writes[j].dstBinding = j;
      desc_writes[j].dstArrayElement = 0;
      desc_writes[j].descriptorType = vk::DescriptorType::eCombinedImageSampler;
      desc_writes[j].descriptorCount = 1;
      desc_writes[j].pImageInfo = &image_infos[j];
    }

    c_.GetDevice().updateDescriptorSets(desc_writes.size(), desc_writes.data(),
                                        0, nullptr);
  }
}

/*
 * TextureArraySet
 */
void TextureArraySet::CreateLayout() {
  vk::DescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  binding.descriptorCount = MAX_TEXTURES;
  binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

  vk::DescriptorSetLayoutCreateInfo layout_info{};
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;

  layout_ = c_.GetDevice().createDescriptorSetLayout(layout_info);
  c_.SetDbgName((uint64_t)(VkDescriptorSetLayout)layout_,
                vk::ObjectType::eDescriptorSetLayout,
                "texture_array_set_layout");
}

void TextureArraySet::Update(const std::vector<npr_graphics::Texture>& textures,
                             const npr_graphics::Texture& default_tex) const {
  assert(textures.size() <= MAX_TEXTURES && count_);

  std::vector<vk::DescriptorImageInfo> image_infos(MAX_TEXTURES);
  for (size_t i = 0; i < textures.size(); ++i) {
    image_infos[i].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[i].imageView = textures[i].GetImageView();
    image_infos[i].sampler = textures[i].GetSampler();
  }

  vk::DescriptorImageInfo default_info{};
  default_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  default_info.imageView = default_tex.GetImageView();
  default_info.sampler = default_tex.GetSampler();

  for (size_t i = textures.size(); i < MAX_TEXTURES; ++i)
    image_infos[i] = default_info[0];

  vk::WriteDescriptorSet desc_write{};
  desc_write.dstSet = sets_[0];
  desc_write.dstBinding = 0;
  desc_write.dstArrayElement = 0;
  desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  desc_write.descriptorCount = MAX_TEXTURES;
  desc_write.pImageInfo = image_infos.data();

  c_.GetDevice().updateDescriptorSets(1, &desc_write, 0, nullptr);
}

void TextureArraySet::Update(uint idx,
                             const npr_graphics::Texture& texture) const {
  assert(idx < MAX_TEXTURES && count_);

  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView = texture.GetImageView();
  imageInfo.sampler = texture.GetSampler();

  vk::WriteDescriptorSet descWrite{};
  descWrite.dstSet = sets_[0];
  descWrite.dstBinding = 0;
  descWrite.dstArrayElement = idx;
  descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descWrite.descriptorCount = 1;
  descWrite.pImageInfo = &imageInfo;

  c_.GetDevice().updateDescriptorSets(1, &descWrite, 0, nullptr);
}

/*
 * ABufferSets
 */
void ABufferSets::CreateLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 3> bindings;

  // abuff_nodes
  bindings[0].binding = 0;
  bindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // abuff_heads
  bindings[1].binding = 1;
  bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

  // abuff_counter (atomics)
  bindings[2].binding = 2;
  bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = vk::ShaderStageFlagBits::eFragment;

  vk::DescriptorSetLayoutCreateInfo layout_info{};
  layout_info.bindingCount = bindings.size();
  layout_info.pBindings = bindings.data();

  layout_ = c_.GetDevice().createDescriptorSetLayout(layout_info);
  c_.SetDbgName((uint64_t)(VkDescriptorSetLayout)layout_,
                vk::ObjectType::eDescriptorSetLayout, "abuffer_set_layout");
}

void ABufferSets::Update(const Resources& res) const {
  const auto& per_frame_res = res.GetResources();
  assert(count_ == per_frame_res.size());

  for (size_t i = 0; i < per_frame_res.size(); ++i) {
    std::array<vk::DescriptorBufferInfo, 3> buffer_infos;

    // abuff_nodes
    buffer_infos[0].buffer = per_frame_res[i].abuff_nodes->GetBuffer();
    buffer_infos[0].offset = 0;
    buffer_infos[0].range = VK_WHOLE_SIZE;

    // abuff_heads
    buffer_infos[1].buffer = per_frame_res[i].abuff_heads->GetBuffer();
    buffer_infos[1].offset = 0;
    buffer_infos[1].range = VK_WHOLE_SIZE;

    // abuff_counter
    buffer_infos[2].buffer = per_frame_res[i].abuff_counter->GetBuffer();
    buffer_infos[2].offset = 0;
    buffer_infos[2].range = VK_WHOLE_SIZE;

    std::array<vk::WriteDescriptorSet, 3> desc_writes;
    for (size_t j = 0; j < 3; ++j) {
      desc_writes[j].dstSet = sets_[i];
      desc_writes[j].dstBinding = j;
      desc_writes[j].dstArrayElement = 0;
      desc_writes[j].descriptorType = vk::DescriptorType::eStorageBuffer;
      desc_writes[j].descriptorCount = 1;
      desc_writes[j].pBufferInfo = &buffer_infos[j];
    }

    c_.GetDevice().updateDescriptorSets(desc_writes.size(), desc_writes.data(),
                                        0, nullptr);
  }
}

}  // namespace npr_graphics
