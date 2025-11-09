#include "world.h"
#include "scene.h"
#include "components.h"

namespace npr_scene {
using namespace npr_graphics;

void World::Reset() {
  entities_.reset();
  instances_.clear();

  camera_query_ = {};
  transform_query_ = {};
  render_opaque_query_ = {};
}

void World::BuildQueries() {
  camera_query_ =
      entities_.query_builder<const CameraTag, const TransformComp>()
          .cached()
          .build();

  transform_query_ =
      entities_.query_builder<TransformComp, const TransformComp*>()
          .cached()
          .detect_changes()
          .term_at(1)
          .parent()
          .build();

  render_opaque_query_ =
      entities_
          .query_builder<const TransformComp, const PrimitiveTag,
                         const MeshComp, const MaterialComp>()
          .cached()
          .detect_changes()
          .term_at(0)
          .parent()
          .build();
}

void World::Update() {
  if (transform_query_.changed()) UpdateTransforms();
  if (render_opaque_query_.changed()) UpdateInstances();
}

void World::UpdateTransforms() {
  entities_.each([](TransformComp& tf) {
    if (!tf.dirty) return;

    glm::mat4 trans_mat = glm::translate(glm::mat4(1.0f), tf.pos);
    glm::mat4 rot_mat = glm::mat4_cast(tf.rot);
    glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), tf.scale);

    tf.local_mat = trans_mat * rot_mat * scale_mat;
    tf.dirty = false;
  });

  transform_query_.each(
      [](TransformComp& out_tf, const TransformComp* parent_tf) {
        if (parent_tf)
          out_tf.global_mat = parent_tf->global_mat * out_tf.local_mat;
        else
          out_tf.global_mat = out_tf.local_mat;
      });
}

void World::UpdateInstances() {
  instances_.clear();

  render_opaque_query_.each([this](const TransformComp& tf, const PrimitiveTag&,
                                   const MeshComp& mesh,
                                   const MaterialComp& mat) {
    if (mesh.vbo_idx == -1) return;

    uint32_t flags = MaterialFlags::kNone;
    if (mat.double_sided) flags |= MaterialFlags::kDoubleSided;
    if (mat.is_opaque) flags |= MaterialFlags::kOpaque;
    if (mat.is_mask) flags |= MaterialFlags::kMask;

    MeshMaterialKey key{
        .mesh = mesh,
        .material = {
            .maps = {mat.color_map_idx, mat.normal_map_idx,
                     mat.metallic_roughness_map_idx, mat.emissive_map_idx},
            .color_factor = mat.color_factor,
            .emissive_factor = mat.emissive_factor,
            .metallic_factor = mat.metallic_factor,
            .roughness_factor = mat.roughness_factor,
            .alpha_cutoff = mat.alpha_cutoff,
            .flags = flags}};

    instances_[key].push_back(
        {.model = tf.global_mat,
         .normal = glm::transpose(glm::inverse(tf.global_mat))});
  });
}

uint World::material_at{0};
uint World::instance_at{0};
void World::RecordOpaque(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
                         uint frame_idx, const Resources& res,
                         const GpuResources& gpu_res,
                         const DescriptorPool& desc_pool) const {
  for (const auto& [key, instances] : instances_) {
    const auto& mat = key.material;
    if ((!(mat.flags & MaterialFlags::kOpaque) &&
         !(mat.flags & MaterialFlags::kMask)) ||
        instances.empty())
      continue;

    cmd_buff.setCullMode((mat.flags & MaterialFlags::kDoubleSided)
                             ? vk::CullModeFlagBits::eNone
                             : vk::CullModeFlagBits::eBack);

    auto& material_ubo = *res.GetResources()[frame_idx].material_ubo;
    material_ubo.Write(material_at, mat);
    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 2,
                                desc_pool.GetMaterialSets().GetSet(frame_idx),
                                material_ubo.GetElemOffset(material_at));

    res.GetResources()[frame_idx].instance_buff->Write(instances, instance_at);

    std::array<vk::DeviceSize, 2> offsets = {0, instance_at * sizeof(Instance)};
    std::array<vk::Buffer, 2> buffers = {
        gpu_res.vbos[key.mesh.vbo_idx].GetBuffer(),
        res.GetResources()[frame_idx].instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      vk::Buffer ibo = gpu_res.ibos[key.mesh.ibo_idx].GetBuffer();
      cmd_buff.bindIndexBuffer(ibo, 0, vk::IndexType::eUint32);
      uint32_t index_count = gpu_res.ibos[key.mesh.ibo_idx].GetCount();
      cmd_buff.drawIndexed(index_count, instances.size(), 0, 0, 0);
    } else {
      uint32_t vertex_count = gpu_res.vbos[key.mesh.vbo_idx].GetCount();
      cmd_buff.draw(vertex_count, instances.size(), 0, 0);
    }

    material_at = (material_at + 1) % MAX_MATERIALS;
    instance_at =
        (instance_at + instances.size()) %
        res.GetResources()[frame_idx].instance_buff->GetMaxInstances();
  }
}

}  // namespace npr_scene
