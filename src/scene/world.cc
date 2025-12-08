#include "world.h"
#include "components.h"

namespace npr_scene {
void World::Reset() {
  entities_.reset();
  instances_.clear();

  cam_query_ = {};
  tf_query_ = {};
  renderable_query_ = {};

  object_query_ = {};

  dir_light_query_ = {};
  point_light_query_ = {};
  spot_light_query_ = {};

  velocity_query_ = {};
  oscillating_query_ = {};
}

void World::BuildQueries() {
  cam_query_ = entities_.query_builder<const CameraTag, const TransformComp>()
                   .cached()
                   .build();
  tf_query_ = entities_.query_builder<TransformComp, const TransformComp*>()
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

  object_query_ =
      entities_.query_builder<const ObjectTag, const TransformComp>()
          .parent()
          .oper(flecs::oper_kind_t::Not)
          .cached()
          .build();

  dir_light_query_ = entities_
                         .query_builder<const DirLightTag, const TransformComp,
                                        const LightComp>()
                         .cached()
                         .detect_changes()
                         .build();
  point_light_query_ =
      entities_
          .query_builder<const PointLightTag, const TransformComp,
                         const LightComp, const RangeComp, BoundingBoxComp>()
          .cached()
          .build();
  spot_light_query_ =
      entities_
          .query_builder<const SpotLightTag, const TransformComp,
                         const LightComp, const RangeComp, const SpotComp,
                         BoundingBoxComp>()
          .cached()
          .build();

  velocity_query_ =
      entities_.query_builder<TransformComp, VelocityComp>().cached().build();
  oscillating_query_ = entities_.query_builder<TransformComp, OscillatingComp>()
                           .cached()
                           .build();
}

void World::Update(const npr_core::FrameTimer* timer_ptr) {
  if (timer_ptr) UpdateMotion(*timer_ptr);

  if (tf_query_.changed()) {
    UpdateTransforms();
    UpdateBB();
  }

  if (renderable_query_.changed()) UpdateInstances();
}

void World::UpdateMotion(const npr_core::FrameTimer& timer) {
  float dt = timer.GetDelta();
  velocity_query_.each([dt](TransformComp& tf, VelocityComp& vel) {
    tf.pos += vel.linear * dt;

    if (glm::length(vel.angular) > 0.001f) {
      float angle = glm::length(vel.angular) * dt;
      glm::vec3 axis = glm::normalize(vel.angular);
      glm::quat rotation = glm::angleAxis(angle, axis);
      tf.rot = tf.rot * rotation;
    }

    tf.dirty = true;
  });

  float elapsed = timer.GetElapsed();
  oscillating_query_.each([elapsed](TransformComp& tf, OscillatingComp& osc) {
    float t = elapsed * osc.freq * glm::two_pi<float>() + osc.phase;
    float offset = osc.amp * std::sin(t);

    if (glm::length(osc.dir) > 0.001f)
      tf.pos = osc.base_pos + normalize(osc.dir) * offset;
    tf.dirty = true;
  });
}

void World::UpdateTransforms() {
  entities_.each([](TransformComp& tf) {
    if (!tf.dirty) return;

    glm::mat3 rot3 = glm::mat3_cast(tf.rot);

    tf.local_mat[0] = glm::vec4(rot3[0] * tf.scale.x, 0.0f);
    tf.local_mat[1] = glm::vec4(rot3[1] * tf.scale.y, 0.0f);
    tf.local_mat[2] = glm::vec4(rot3[2] * tf.scale.z, 0.0f);
    tf.local_mat[3] = glm::vec4(tf.pos, 1.0f);

    tf.dirty = false;
  });

  tf_query_.each([](TransformComp& out_tf, const TransformComp* parent_tf) {
    out_tf.prev_glob_mat = out_tf.glob_mat;

    if (parent_tf)
      out_tf.glob_mat = parent_tf->glob_mat * out_tf.local_mat;
    else
      out_tf.glob_mat = out_tf.local_mat;

    if (!out_tf.init) {  // first update
      out_tf.prev_glob_mat = out_tf.glob_mat;
      out_tf.init = true;
    }
  });
}

void World::CalcBB(const TransformComp& tf, BoundingBoxComp& bb) {
  glm::vec3 local_center = (bb.min_pos + bb.max_pos) * 0.5f;
  glm::vec3 local_extent = (bb.max_pos - bb.min_pos) * 0.5f;

  glm::mat3 model3 = glm::mat3(tf.glob_mat);
  glm::vec3 scale;

  scale.x = glm::length(model3[0]);
  scale.y = glm::length(model3[1]);
  scale.z = glm::length(model3[2]);

  model3[0] = glm::normalize(model3[0]);
  model3[1] = glm::normalize(model3[1]);
  model3[2] = glm::normalize(model3[2]);

  bb.center = tf.glob_mat * glm::vec4(local_center, 1.0f);
  bb.extent = local_extent * scale;
  bb.inv_rot = glm::transpose(model3);
}

void World::UpdateBB() {
  renderable_query_.each([](const TransformComp& tf, const PrimitiveTag&,
                            const MeshComp&, const MaterialComp&,
                            BoundingBoxComp& bb) { CalcBB(tf, bb); });

  point_light_query_.each([](const PointLightTag&, const TransformComp& tf,
                             const LightComp&, const RangeComp&,
                             BoundingBoxComp& bb) { CalcBB(tf, bb); });

  spot_light_query_.each([](const SpotLightTag&, const TransformComp& tf,
                            const LightComp&, const RangeComp&, const SpotComp&,
                            BoundingBoxComp& bb) { CalcBB(tf, bb); });
}

void World::UpdateInstances() {
  for (auto& [key, list] : instances_) list.clear();

  renderable_query_.each([this](const TransformComp& tf, const PrimitiveTag&,
                                const MeshComp& mesh, const MaterialComp& mat,
                                const BoundingBoxComp& bb) {
    if (mesh.vbo_idx == -1) return;
    instances_[{mesh, mat}].push_back({tf, bb});
  });
}

}  // namespace npr_scene
