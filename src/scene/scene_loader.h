#ifndef SCENE_LOADER_H_
#define SCENE_LOADER_H_

#include "scene.h"
#include "components.h"

#include "graphics/renderer.h"

#include <tiny_gltf.h>

namespace npr_scene {

class SceneLoader {
  struct LoaderCache {
    bool light_supp{false};

    std::unordered_set<int> nodes;
    std::map<std::pair<int /*mesh_idx*/, int /*prim_idx*/>, MeshComp> meshes;
    std::unordered_map<int /*material_idx*/, MaterialComp> materials;
  };

 public:
  static void LoadAsync(const npr_graphics::Renderer& renderer, Scene& scene,
                        npr_core::TaskManager& tasks,
                        std::function<void()> on_loaded,
                        const std::string& filepath,
                        const std::string& scene_name = "");

  static void Load(const npr_graphics::Renderer& renderer, Scene& scene,
                   const std::string& filepath,
                   const std::string& scene_name = "");

 private:
  static bool LoadScene(const npr_graphics::Renderer& renderer, Scene& scene,
                        const std::string& filepath,
                        const std::string& scene_name);

  static void LoadEntity(Scene& scene, flecs::entity parent_ent,
                         const tinygltf::Model& model, const int node_idx,
                         LoaderCache& cache);

  static void LoadObject(Scene& scene, flecs::entity node_ent,
                         const tinygltf::Model& model,
                         const tinygltf::Node& node, LoaderCache& cache);
  static void LoadCamera(flecs::entity node_ent, const tinygltf::Model& model,
                         const tinygltf::Node& node);

  static void AddTransformComp(flecs::entity ent, const tinygltf::Node& node);
  static void AddMeshComp(Scene& scene, flecs::entity ent,
                          const tinygltf::Model& model,
                          const tinygltf::Node& node, const int prim_idx,
                          LoaderCache& cache);
  static void AddMaterialComp(Scene& scene, flecs::entity ent,
                              const tinygltf::Model& model,
                              const tinygltf::Node& node, const int prim_idx,
                              LoaderCache& cache);

  static int LoadTexture(Scene& scene, const tinygltf::Model& model,
                         const tinygltf::Material& material,
                         const npr_graphics::TextureType tex_type);
  static npr_graphics::SamplerProps LoadSampler(
      const tinygltf::Model& model, const tinygltf::Texture& texture);

  // helpers
  static int GetSceneIdx(const tinygltf::Model& model,
                         const std::string& scene_name);
  static int GetLightIdx(const tinygltf::Node& node, const LoaderCache& cache);
  static int GetAccessorIdx(const tinygltf::Primitive& primitive,
                            const std::string& attr_name, bool req);

  template <typename T>
  static const T* GetVertexData(const tinygltf::Model& model, const int acc_idx,
                                const int expected_type,
                                const int expected_comp_type,
                                const std::string& attr_name);
  template <typename T>
  static const T* GetIndexData(const tinygltf::Model& model, const int acc_idx);

  static int GetTexUvSetIdx(const tinygltf::Material& material);
  static int GetTexIdx(const tinygltf::Material& material,
                       npr_graphics::TextureType tex_type);
};

}  // namespace npr_scene

#endif  // SCENE_LOADER_H_
