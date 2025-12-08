#ifndef WORLD_H_
#define WORLD_H_

#include "core/frame_timer.h"
#include "instance_map.h"
#include <flecs.h>

namespace npr_scene {

class World : npr_core::NonCopyable {
  friend class SceneLoader;
  friend class Scene;

 private:
  void Update(const npr_core::FrameTimer* timer = nullptr);
  void Reset();
  void BuildQueries();

  void UpdateMotion(const npr_core::FrameTimer& timer);
  void UpdateTransforms();
  void UpdateInstances();
  void UpdateBB();

  static void CalcBB(const TransformComp& tf, BoundingBoxComp& bb);

 private:
  flecs::world entities_;
  InstanceMap instances_;

  flecs::query<const CameraTag, const TransformComp> cam_query_;
  flecs::query<TransformComp,         // transform to update
               const TransformComp*>  // parent transform
      tf_query_;
  flecs::query<const TransformComp,  // parent transform
               const PrimitiveTag, const MeshComp, const MaterialComp,
               BoundingBoxComp>
      renderable_query_;

  flecs::query<const ObjectTag, const TransformComp> object_query_;

  // light queries
  flecs::query<const DirLightTag, const TransformComp, const LightComp>
      dir_light_query_;
  flecs::query<const PointLightTag, const TransformComp, const LightComp,
               const RangeComp, BoundingBoxComp>
      point_light_query_;
  flecs::query<const SpotLightTag, const TransformComp, const LightComp,
               const RangeComp, const SpotComp, BoundingBoxComp>
      spot_light_query_;

  flecs::query<TransformComp, VelocityComp> velocity_query_;
  flecs::query<TransformComp, OscillatingComp> oscillating_query_;
};

}  // namespace npr_scene

#endif  // WORLD_H_
