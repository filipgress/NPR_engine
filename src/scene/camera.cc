#include "camera.h"
#include "components.h"

namespace npr_scene {

Camera::Camera(flecs::entity ent) {
  SetEntity(ent);
  curr_ = dest_;

  SetViewMat();
  frustrum_.Update(proj_ * view_);
}

void Camera::SetEntity(flecs::entity ent) {
  ent_ = ent;

  const auto& transform = ent_.get<TransformComp>();
  dest_.pos = transform.pos;
  dest_.dist = 5.0f;

  glm::vec3 front = transform.rot * glm::vec3(0.0f, 0.0f, -1.0f);
  dest_.target = dest_.pos + front * dest_.dist;

  glm::vec3 vec = (dest_.pos - dest_.target) / dest_.dist;
  dest_.theta = glm::asin(vec.y);
  dest_.phi = glm::atan(vec.x, vec.z);

  dest_.phi = glm::mod(dest_.phi, glm::two_pi<float>());
  dest_.theta = glm::clamp(dest_.theta, -props_.max_theta, props_.max_theta);

  SetProjMat();
}

void Camera::SetMode(CameraMode mode) {
  props_.mode = mode;

  if (mode == CameraMode::kFree) {
    curr_.pos = CalcOrbitPos(curr_);
    dest_.pos = CalcOrbitPos(dest_);
  }
}

float Camera::GetAspectRatio() const {
  if (ent_.has<PerspectiveComp>()) {
    return ent_.get<PerspectiveComp>().aspect;
  } else if (ent_.has<OrthographicComp>()) {
    auto& ortho = ent_.get<OrthographicComp>();
    return ortho.xmag / ortho.ymag;
  }
  assert(false);
}

void Camera::SetTrackTarget(const glm::vec3& target) {
  if (props_.mode == CameraMode::kOrbit) dest_.pos = CalcOrbitPos(dest_);

  dest_.target = target;
  dest_.dist = glm::clamp(glm::distance(dest_.pos, target), props_.min_dist,
                          props_.max_dist);

  glm::vec3 vec = (dest_.pos - target) / dest_.dist;
  dest_.theta = glm::asin(vec.y);
  dest_.phi = glm::atan(vec.x, vec.z);

  dest_.phi = glm::mod(dest_.phi, glm::two_pi<float>());
  dest_.theta = glm::clamp(dest_.theta, -props_.max_theta, props_.max_theta);

  if (props_.mode == CameraMode::kFree) {
    curr_.dist = dest_.dist;
    curr_.phi = dest_.phi;
    curr_.theta = dest_.theta;
  }

  props_.mode = CameraMode::kOrbit;
}

void Camera::SetProjMat() {
  if (ent_.has<PerspectiveComp>()) {
    const auto& persp = ent_.get<PerspectiveComp>();
    proj_ = glm::perspective(persp.fov, persp.aspect, persp.near, persp.far);

  } else if (ent_.has<OrthographicComp>()) {
    const auto& ortho = ent_.get<OrthographicComp>();
    proj_ = glm::ortho(-ortho.xmag, ortho.xmag, -ortho.ymag, ortho.ymag,
                       ortho.near, ortho.far);
  }
}

void Camera::SetViewMat() {
  if (props_.mode == CameraMode::kOrbit) {
    view_ = glm::lookAt(CalcOrbitPos(curr_), curr_.target,
                        glm::vec3(0.0f, 1.0f, 0.0f));
  } else {
    view_ = glm::lookAt(curr_.pos, curr_.pos + CalcFpsFront(curr_),
                        glm::vec3(0.0f, 1.0f, 0.0f));
  }
}

void Camera::Update(float dt) {
  float t = glm::clamp(props_.anim_factor * dt, 0.0f, 1.0f);

  if (props_.mode == CameraMode::kOrbit) {
    curr_.target = glm::mix(curr_.target, dest_.target, t);
    curr_.dist = glm::mix(curr_.dist, dest_.dist, t);

    float phi_diff = dest_.phi - curr_.phi;
    if (phi_diff > glm::pi<float>())
      phi_diff -= glm::two_pi<float>();
    else if (phi_diff < -glm::pi<float>())
      phi_diff += glm::two_pi<float>();

    curr_.phi = glm::mod(curr_.phi + phi_diff * t, glm::two_pi<float>());
    curr_.theta = glm::mix(curr_.theta, dest_.theta, t);

  } else {
    curr_.pos = glm::mix(curr_.pos, dest_.pos, t);

    float phi_diff = dest_.phi - curr_.phi;
    if (phi_diff > glm::pi<float>())
      phi_diff -= glm::two_pi<float>();
    else if (phi_diff < -glm::pi<float>())
      phi_diff += glm::two_pi<float>();

    curr_.phi = glm::mod(curr_.phi + phi_diff * t, glm::two_pi<float>());
    curr_.theta = glm::mix(curr_.theta, dest_.theta, t);
  }

  SetViewMat();
  frustrum_.Update(proj_ * view_);
}

void Camera::Orbit(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;

  dest_.phi -= delta.x * props_.orbit_factor;
  dest_.theta += delta.y * props_.orbit_factor;

  dest_.phi = glm::mod(dest_.phi, glm::two_pi<float>());
  dest_.theta = glm::clamp(dest_.theta, -props_.max_theta, props_.max_theta);
}

void Camera::Pan(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;

  glm::vec3 front = glm::normalize(dest_.target - CalcOrbitPos(dest_));
  glm::vec3 right =
      glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
  glm::vec3 up = glm::normalize(glm::cross(right, front));

  dest_.target +=
      props_.pan_factor * dest_.dist * (right * -delta.x + up * delta.y);
}

void Camera::Zoom(float delta) {
  if (!delta) return;
  dest_.dist = glm::clamp(dest_.dist * std::exp(-delta * props_.zoom_factor),
                          props_.min_dist, props_.max_dist);
}

void Camera::Move(glm::vec3 delta) {
  if (!delta.x && !delta.y && !delta.z) return;

  glm::vec3 front = CalcFpsFront(dest_);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::normalize(glm::cross(front, up));

  dest_.pos +=
      props_.move_factor * (right * delta.x + up * delta.y + front * delta.z);
}

void Camera::Rotate(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;

  dest_.phi -= delta.x * props_.rotate_factor;
  dest_.theta += delta.y * props_.rotate_factor;

  dest_.phi = glm::mod(dest_.phi, glm::two_pi<float>());
  dest_.theta = glm::clamp(dest_.theta, -props_.max_theta, props_.max_theta);
}

glm::vec3 Camera::CalcFpsFront(const CameraState& state) const {
  float sin_theta = std::sin(state.theta);
  float cos_theta = std::cos(state.theta);
  float sin_phi = std::sin(state.phi);
  float cos_phi = std::cos(state.phi);

  return -glm::vec3(sin_phi * cos_theta, sin_theta, cos_phi * cos_theta);
}

glm::vec3 Camera::CalcOrbitPos(const CameraState& state) const {
  float sin_theta = std::sin(state.theta);
  float cos_theta = std::cos(state.theta);
  float sin_phi = std::sin(state.phi);
  float cos_phi = std::cos(state.phi);

  return state.target + state.dist * glm::vec3(sin_phi * cos_theta, sin_theta,
                                               cos_phi * cos_theta);
}

}  // namespace npr_scene
