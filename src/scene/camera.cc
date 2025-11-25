#include "camera.h"

#include "components.h"

namespace npr_scene {

Camera::Camera(ProjProps proj_props, CameraProps cam_props)
    : proj_props_(proj_props), props_(cam_props) {
  SetProjMat();
  SetViewMat();
  frustum_.Update(proj_ * view_);
}

float Camera::GetAspect() const {
  if (!ent_.is_valid()) return proj_props_.aspect;
  if (ent_.has<PerspectiveComp>()) return ent_.get<PerspectiveComp>().aspect;
  if (ent_.has<OrthographicComp>()) {
    auto& ortho = ent_.get<OrthographicComp>();
    return ortho.xmag / ortho.ymag;
  }

  assert(false);
}

void Camera::SetAspect(float aspect) {
  proj_props_.aspect = aspect;

  if (ent_.is_valid()) return;

  SetProjMat();
  frustum_.Update(proj_ * view_);
}

void Camera::SetEntity(flecs::entity ent) {
  if (!ent.is_valid() || !ent.has<CameraTag>()) return;

  ent_ = ent;

  const auto& transform = ent_.get<TransformComp>();
  dest_.pos = transform.pos;
  dest_.front = transform.rot * glm::vec3(0.0f, 0.0f, -1.0f);
  target_ = dest_.pos + dest_.front * 5.0f;

  SetProjMat();
  frustum_.Update(proj_ * view_);
}

void Camera::SetTrackTarget(flecs::entity ent) {
  if (!ent.is_valid() || !ent.has<TransformComp>()) return;

  const auto& tf = ent.get<TransformComp>();
  glm::vec3 target = glm::vec3(tf.glob_mat[3]);
  SetTrackTarget(target);
}

void Camera::SetTrackTarget(const glm::vec3& target) {
  target_ = target;
  dest_.front = glm::normalize(target_ - dest_.pos);

  InvalidateEntity();
}

void Camera::SetProjMat() {
  if (!ent_.is_valid()) {
    proj_ = glm::perspective(proj_props_.fov, proj_props_.aspect,
                             proj_props_.near, proj_props_.far);

  } else if (ent_.has<PerspectiveComp>()) {
    const auto& persp = ent_.get<PerspectiveComp>();
    proj_ = glm::perspective(persp.fov, persp.aspect, persp.near, persp.far);

  } else if (ent_.has<OrthographicComp>()) {
    const auto& ortho = ent_.get<OrthographicComp>();
    proj_ = glm::ortho(-ortho.xmag, ortho.xmag, -ortho.ymag, ortho.ymag,
                       ortho.near, ortho.far);
  }
  proj_[1][1] *= -1;  // flip y for vulkan
}

void Camera::SetViewMat() {
  view_ = glm::lookAt(curr_.pos, curr_.pos + curr_.front,
                      glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::Update(float dt) {
  if (glm::length(curr_.pos - dest_.pos) < 0.001f &&
      glm::length(curr_.front - dest_.front) < 0.001f)
    return;

  if (props_.mode == CameraMode::kOrbit) {
    float t = glm::clamp(props_.orbit_anim_factor * dt, 0.0f, 1.0f);
    curr_.pos = glm::mix(curr_.pos, dest_.pos, t);
    curr_.front = glm::normalize(glm::mix(curr_.front, dest_.front, t));

  } else if (props_.mode == CameraMode::kFree) {
    curr_.pos = dest_.pos;
    curr_.front = dest_.front;
  }

  SetViewMat();
  frustum_.Update(proj_ * view_);
}

void Camera::InvalidateEntity() {
  if (!ent_.is_valid()) return;

  ent_ = flecs::entity();

  SetProjMat();
  frustum_.Update(proj_ * view_);
}

void Camera::Orbit(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;
  InvalidateEntity();

  glm::vec3 offset = glm::normalize(dest_.pos - target_);
  float theta = glm::asin(offset.y);
  float phi = std::atan2(offset.z, offset.x);

  theta += delta.y * props_.orbit_factor;
  phi += delta.x * props_.orbit_factor;

  theta = glm::clamp(theta, -props_.max_theta, props_.max_theta);
  phi = glm::mod(phi, glm::two_pi<float>());

  float cos_theta = std::cos(theta);
  glm::vec3 dir = glm::normalize(
      glm::vec3(cos(phi) * cos_theta, sin(theta), sin(phi) * cos_theta));

  dest_.pos = target_ + glm::distance(dest_.pos, target_) * dir;
  dest_.front = -dir;
}

void Camera::Pan(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;
  InvalidateEntity();

  glm::vec3 right =
      glm::normalize(glm::cross(dest_.front, glm::vec3(0.0f, 1.0f, 0.0f)));
  glm::vec3 up = glm::normalize(glm::cross(right, dest_.front));

  glm::vec3 diff = props_.pan_factor * glm::distance(dest_.pos, target_) *
                   (right * -delta.x + up * delta.y);
  dest_.pos += diff;
  target_ += diff;
}

void Camera::Zoom(float delta) {
  if (!delta) return;
  InvalidateEntity();

  float dist = glm::clamp(
      glm::distance(dest_.pos, target_) * std::exp(-delta * props_.zoom_factor),
      props_.min_dist, props_.max_dist);
  dest_.pos = target_ - dist * dest_.front;
}

void Camera::Move(glm::vec3 delta, float dt) {
  if (!delta.x && !delta.y && !delta.z) return;
  InvalidateEntity();

  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::normalize(glm::cross(dest_.front, up));
  dest_.pos += dt * props_.move_factor *
               (right * delta.x + up * delta.y + dest_.front * delta.z);
}

void Camera::Rotate(glm::vec2 delta) {
  if (!delta.x && !delta.y) return;
  InvalidateEntity();

  glm::vec3 dir = -dest_.front;
  float theta = glm::asin(dir.y);
  float phi = std::atan2(dir.z, dir.x);

  theta += delta.y * props_.rotate_factor;
  phi += delta.x * props_.rotate_factor;

  theta = glm::clamp(theta, -props_.max_theta, props_.max_theta);
  phi = glm::mod(phi, glm::two_pi<float>());

  float cos_theta = std::cos(theta);
  dest_.front = -glm::normalize(
      glm::vec3(cos(phi) * cos_theta, sin(theta), sin(phi) * cos_theta));
}

}  // namespace npr_scene
