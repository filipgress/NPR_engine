#include "shader_manager.h"

namespace npr_graphics {
ShaderManager::ShaderManager(const Context& ctx)
    : quad_vert_{ctx, "shaders/quad_vert.spv", "../shaders/quad.vert"},
      gbuff_vert_{ctx, "shaders/gbuff_vert.spv", "../shaders/gbuff.vert"},
      light_vert_{ctx, "shaders/light_vert.spv", "../shaders/light.vert"},

      gbuff_frag_{ctx, "shaders/gbuff_frag.spv", "../shaders/gbuff.frag"},
      ao_frag_{ctx, "shaders/ao_frag.spv", "../shaders/ao.frag"},
      blur_frag_{ctx, "shaders/blur_frag.spv", "../shaders/blur.frag"},

      dir_light_frag_{ctx, "shaders/dir_light_frag.spv",
                      "../shaders/dir_light.frag"},
      point_light_frag_{ctx, "shaders/point_light_frag.spv",
                        "../shaders/point_light.frag"},
      spot_light_frag_{ctx, "shaders/spot_light_frag.spv",
                       "../shaders/spot_light.frag"},

      dir_int_frag_{ctx, "shaders/dir_int_frag.spv", "../shaders/dir_int.frag"},
      point_int_frag_{ctx, "shaders/point_int_frag.spv",
                      "../shaders/point_int.frag"},
      spot_int_frag_{ctx, "shaders/spot_int_frag.spv",
                     "../shaders/spot_int.frag"},
      shading_frag_{ctx, "shaders/shading_frag.spv", "../shaders/shading.frag"},

      abuff_fill_frag_{ctx, "shaders/abuff_fill_frag.spv",
                       "../shaders/abuff_fill.frag"},
      abuff_res_frag_{ctx, "shaders/abuff_res_frag.spv",
                      "../shaders/abuff_res.frag"},
      wboit_acc_frag_{ctx, "shaders/wboit_acc_frag.spv",
                      "../shaders/wboit_acc.frag"},
      wboit_res_frag_{ctx, "shaders/wboit_res_frag.spv",
                      "../shaders/wboit_res.frag"},
      post_process_frag_{ctx, "shaders/post_process_frag.spv",
                         "../shaders/post_process.frag"},
      bright_extract_frag_{ctx, "shaders/bright_extract_frag.spv",
                           "../shaders/bright_extract.frag"},
      coc_extract_frag_{ctx, "shaders/coc_extract_frag.spv",
                        "../shaders/coc_extract.frag"},
      dof_poisson_frag_{ctx, "shaders/dof_poisson_frag.spv",
                        "../shaders/dof_poisson.frag"},
      motion_frag_{ctx, "shaders/motion_blur_frag.spv",
                   "../shaders/motion_blur.frag"},
      swap_frag_{ctx, "shaders/swap_frag.spv", "../shaders/swap.frag"},
      shaders_{
          &quad_vert_,           &gbuff_vert_,       &light_vert_,
          &gbuff_frag_,          &ao_frag_,          &blur_frag_,
          &dir_light_frag_,      &point_light_frag_, &spot_light_frag_,
          &dir_int_frag_,        &point_int_frag_,   &spot_int_frag_,
          &shading_frag_,        &abuff_fill_frag_,  &abuff_res_frag_,
          &wboit_acc_frag_,      &wboit_res_frag_,   &post_process_frag_,
          &bright_extract_frag_, &coc_extract_frag_, &dof_poisson_frag_,
          &motion_frag_,         &swap_frag_,
      } {}

void ShaderManager::Recompile() {
  if (!kEnableShaderReload) return;

  INFO("Recompiling shaders");
  for (auto* shader : shaders_) shader->ReloadAsync();
}

bool ShaderManager::IsDirty() {
  if (!kEnableShaderReload) return false;

  bool dirty{false};
  for (auto* shader : shaders_) {
    if (shader->IsDirty()) {
      shader->CreateShaderModule();
      dirty = true;
    }
  }

  return dirty;
}

}  // namespace npr_graphics
