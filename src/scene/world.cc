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
  renderable_query_ = {};
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
  renderable_query_ =
      entities_
          .query_builder<const TransformComp, const PrimitiveTag,
                         const MeshComp, const MaterialComp, BoundingBoxComp>()
          .cached()
          .detect_changes()
          .term_at(0)
          .parent()
          .build();
}

void World::Update() {
  if (transform_query_.changed()) {
    UpdateTransforms();
    UpdateBoundingBoxes();
  }

  if (renderable_query_.changed()) UpdateInstances();
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

void World::UpdateBoundingBoxes() {
  renderable_query_.each([](const TransformComp& tf, const PrimitiveTag&,
                            const MeshComp&, const MaterialComp&,
                            BoundingBoxComp& bb) {
    glm::vec3 local_center = (bb.min_pos + bb.max_pos) * 0.5f;
    glm::vec3 local_extent = (bb.max_pos - bb.min_pos) * 0.5f;

    glm::mat3 model3 = glm::mat3(tf.global_mat);
    glm::vec3 scale;

    scale.x = glm::length(model3[0]);
    scale.y = glm::length(model3[1]);
    scale.z = glm::length(model3[2]);

    model3[0] = glm::normalize(model3[0]);
    model3[1] = glm::normalize(model3[1]);
    model3[2] = glm::normalize(model3[2]);

    bb.center = tf.global_mat * glm::vec4(local_center, 1.0f);
    bb.extent = local_extent * scale;
    bb.inv_rot = glm::transpose(model3);
  });
}

void World::UpdateInstances() {
  instances_.clear();

  renderable_query_.each([this](const TransformComp& tf, const PrimitiveTag&,
                                const MeshComp& mesh, const MaterialComp& mat,
                                const BoundingBoxComp& bb) {
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

    instances_[key].push_back({tf, bb});
  });
}

uint World::material_at{0};
uint World::instance_at{0};
void World::Record(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
                   const npr_graphics::FrameResources& frame_res,
                   uint32_t set_idx, vk::DescriptorSet material_set,
                   const Frustum& frustum, const GpuResources& gpu_res,
                   bool set_culling, KeyPredFn pred) const {
  for (const auto& [key, instances] : instances_) {
    const auto& mat = key.material;
    if (instances.empty() || (pred && !pred(key))) continue;

    std::vector<InstanceData> visible_instances;
    visible_instances.reserve(instances.size());

    for (const auto& [tf, bb] : instances) {
      if (!frustum.IsVisible(bb)) continue;
      visible_instances.push_back(
          {.model = tf.global_mat,
           .normal = glm::transpose(glm::inverse(tf.global_mat))});
    }

    if (visible_instances.empty()) continue;

    if (set_culling)
      cmd_buff.setCullMode((mat.flags & MaterialFlags::kDoubleSided)
                               ? vk::CullModeFlagBits::eNone
                               : vk::CullModeFlagBits::eBack);

    auto& material_ubo = *frame_res.material_ubo;
    material_ubo.Write(material_at, mat);
    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                                set_idx, material_set,
                                material_ubo.GetElemOffset(material_at));

    // wrap-around for instance_at
    if (instance_at + visible_instances.size() >= MAX_INSTANCES)
      instance_at = 0;
    frame_res.instance_buff->Write(visible_instances, instance_at);

    std::array<vk::DeviceSize, 2> offsets = {
        0, instance_at * sizeof(InstanceData)};
    std::array<vk::Buffer, 2> buffers = {
        gpu_res.vbos[key.mesh.vbo_idx].GetBuffer(),
        frame_res.instance_buff->GetBuffer()};

    cmd_buff.bindVertexBuffers(0, buffers.size(), buffers.data(),
                               offsets.data());

    if (key.mesh.ibo_idx != -1) {
      vk::Buffer ibo = gpu_res.ibos[key.mesh.ibo_idx].GetBuffer();
      cmd_buff.bindIndexBuffer(ibo, 0, vk::IndexType::eUint32);
      uint32_t index_count = gpu_res.ibos[key.mesh.ibo_idx].GetCount();
      cmd_buff.drawIndexed(index_count, visible_instances.size(), 0, 0, 0);
    } else {
      uint32_t vertex_count = gpu_res.vbos[key.mesh.vbo_idx].GetCount();
      cmd_buff.draw(vertex_count, visible_instances.size(), 0, 0);
    }

    material_at = (material_at + 1) % MAX_MATERIALS;
    instance_at = (instance_at + visible_instances.size()) % MAX_INSTANCES;
  }
}

}  // namespace npr_scene
