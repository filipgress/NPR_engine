#ifndef SETTINGS_H
#define SETTINGS_H

namespace npr_graphics {
enum class TransparencyMode { kNone, kABuff, kWBoit };
struct RenderSettings {
  vk::Extent2D target_size{500, 400};

  glm::vec3 ambient_color{0.3f, 0.3f, 0.3f};
  float ambient_intensity{0.225f};

  glm::vec3 rim_color{1.0f, 1.0f, 1.0f};
  float rim_intensity{0.0f};

  float diff_int{1.0f};
  float spec_int{1.0f};
  float rim_power{4.0f};
  bool inv_rim{false};
  bool is_pbr{true};

  bool enable_ssao{false};
  float ssao_radius{0.5f};
  float ssao_bias{0.025f};

  TransparencyMode trans_mode{TransparencyMode::kWBoit};
  float alpha_cutoff{0.001f};

  uint32_t abuff_avg_nodes{4};
  uint32_t abuff_sorted_nodes{8};

  float wboit_alpha_multiplier{10.0f};
  float wboit_alpha_power{3.0};
  float wboit_depth_factor{0.9};
  float wboit_depth_power{3.0};
  float wboit_weight_min{1e-2};
  float wboit_weight_max{3e3};

  bool enable_post_process{false};

  bool enable_bloom{false};
  float bloom_threshold{1.0f};
  float bloom_soft_threshold{0.5f};
  float bloom_intensity{1.0f};

  bool enable_dof{false};
  uint dof_debug_mode{0};
  float dof_focus_distance{10.0f};
  float dof_focus_range{1.8f};
  float dof_near_int{1.0f};
  float dof_far_int{1.0f};
  float dof_near_falloff{4.0f};
  float dof_far_falloff{4.0f};

  float dof_blur_radius{5.0f};
  float dof_coc_threshold{0.05f};
  float dof_coc_falloff{10.0f};

  bool dirty_target_size{false};
  bool dirty_abuff_size{false};
};
}  // namespace npr_graphics

#endif  // SETTINGS_H
