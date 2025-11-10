#ifndef CAMERA_H_
#define CAMERA_H_

#include "frustum.h"
#include "graphics/resources.h"

#include <flecs.h>
#include <glm/gtc/quaternion.hpp>

namespace npr_scene {

struct ProjProps {
  float fov{45.0f};
  float aspect{16.0f / 9.0f};
  float near{0.1f};
  float far{100.0f};
};

enum class CameraMode { kFree, kOrbit };
struct CameraProps {
  float orbit_factor{4.5f};
  float pan_factor{1.3f};
  float zoom_factor{0.02f};
  float move_factor{10.0f};
  float rotate_factor{750.0f};

  float orbit_anim_factor{11.0f};
  float free_anim_factor{17.0f};

  float min_dist{0.5f};
  float max_dist{100.0f};
  float max_theta{glm::pi<float>() * 0.499f};

  CameraMode mode{CameraMode::kOrbit};
};

struct CameraState {
  // using coordinate system: right=+X, up=+Y, forward=-Z
  glm::vec3 pos{0.0f, 0.0f, 5.0f};
  glm::vec3 front{0.0f, 0.0f, -1.0f};
};

class Camera {
 public:
  Camera(ProjProps proj_props = ProjProps(),
         CameraProps cam_props = CameraProps());
  Camera(float aspect) : Camera{{.aspect = aspect}} {}

  CameraMode GetMode() const { return props_.mode; }
  void SetMode(CameraMode mode) { props_.mode = mode; }

  float GetAspect() const;
  void SetAspect(float aspect);

  const Frustum& GetFrustum() const { return frustum_; }
  npr_graphics::CameraUnif GetCameraUnif() const {
    return {view_, proj_, proj_ * view_};
  }

  void SetEntity(flecs::entity ent);
  void SetTrackTarget(const glm::vec3& target);

  void Update(float dt);

  void Orbit(glm::vec2 delta);
  void Pan(glm::vec2 delta);
  void Zoom(float delta);

  void Move(glm::vec3 delta, float dt);
  void Rotate(glm::vec2 delta, float dt);

 private:
  void SetProjMat();
  void SetViewMat();
  void InvalidateEntity();

 private:
  glm::mat4 proj_{1.0f};
  glm::mat4 view_{1.0f};

  CameraState curr_{};
  CameraState dest_{};
  glm::vec3 target_{0.0f};

  ProjProps proj_props_{};
  CameraProps props_{};

  flecs::entity ent_{};
  Frustum frustum_{};
};

}  // namespace npr_scene

#endif  // CAMERA_H_
