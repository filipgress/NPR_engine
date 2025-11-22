#ifndef WORLD_H_
#define WORLD_H_

#include "instance_map.h"
#include <flecs.h>

namespace npr_scene {

class World : npr_core::NonCopyable {
  friend class SceneLoader;
  friend class Scene;

 private:
  void Update();
  void Reset();
  void BuildQueries();

  void UpdateTransforms();
  void UpdateBB();
  void UpdateInstances();

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

  // light queries
  flecs::query<const DirLightTag, const TransformComp, const LightComp>
      dir_light_query_;
  flecs::query<const PointLightTag, const TransformComp, const LightComp,
               const RangeComp>
      point_light_query_;
  flecs::query<const SpotLightTag, const TransformComp, const LightComp,
               const RangeComp, const SpotComp>
      spot_light_query_;
};

}  // namespace npr_scene

#endif  // WORLD_H_
