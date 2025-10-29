#ifndef CAMERA_H_
#define CAMERA_H_

#include "frustrum.h"
#include "graphics/resources.h"

#include <flecs.h>
#include <glm/gtc/quaternion.hpp>

namespace npr_scene {

enum class CameraMode { kFree, kOrbit };
struct CameraProps {
  float orbit_factor{4.5f};
  float pan_factor{1.3f};
  float zoom_factor{0.02f};
  float move_factor{0.1f};
  float rotate_factor{1.5f};
  float anim_factor{11.0f};

  float min_dist{0.5f};
  float max_dist{100.0f};
  float max_theta{glm::pi<float>() * 0.499f};

  CameraMode mode{CameraMode::kOrbit};
};

struct CameraState {
  glm::vec3 pos{0.0f, 0.0f, 5.0f};

  glm::vec3 target{0.0f};
  float dist{5.0f};
  float phi{0.0f};
  float theta{0.0f};
};

class Camera {
 public:
  Camera(flecs::entity ent);

  CameraMode GetMode() const { return props_.mode; }
  void SetMode(CameraMode mode);

  float GetAspectRatio() const;
  const Frustrum& GetFrustrum() const { return frustrum_; }
  npr_graphics::CameraUnif GetCameraUnif() {
    return npr_graphics::CameraUnif{view_, proj_, proj_ * view_};
  }

  void SetEntity(flecs::entity ent);
  void SetTrackTarget(const glm::vec3& target);

  void Update(float dt);

  void Orbit(glm::vec2 delta);
  void Pan(glm::vec2 delta);
  void Zoom(float delta);

  void Move(glm::vec3 delta);
  void Rotate(glm::vec2 delta);

 private:
  void SetProjMat();
  void SetViewMat();

  glm::vec3 CalcFpsFront(const CameraState& state) const;
  glm::vec3 CalcOrbitPos(const CameraState& state) const;

 private:
  glm::mat4 proj_{1.0f};
  glm::mat4 view_{1.0f};

  CameraState curr_{};
  CameraState dest_{};

  CameraProps props_;
  flecs::entity ent_;

  Frustrum frustrum_;
};

}  // namespace npr_scene

#endif  // CAMERA_H_
