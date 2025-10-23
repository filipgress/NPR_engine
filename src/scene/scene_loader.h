#ifndef SCENE_LOADER_H_
#define SCENE_LOADER_H_

#include "scene.h"
#include "components.h"

#include "graphics/vulkan_context.h"

#include <tiny_gltf.h>

namespace npr_scene {

class SceneLoader {
  struct LoaderCache {
    const npr_graphics::VulkanContext& context;
    vk::CommandBuffer cmd_buff{nullptr};

    std::string name;
    bool light_supp{false};

    std::unordered_map<int, MeshComp> meshes;

    LoaderCache(const npr_graphics::VulkanContext& context,
                const std::string& scene_file)
        : context{context} {
      name = std::filesystem::path(scene_file).stem().string();
    }
  };

 public:
  static void LoadAsync(const npr_graphics::VulkanContext& context,
                        Scene& scene, const std::string& scene_file);
  static void Load(const npr_graphics::VulkanContext& context, Scene& scene,
                   const std::string& scene_file);

 private:
  static bool LoadScene(const npr_graphics::VulkanContext& context,
                        Scene& scene, const std::string& scene_file);
  static void PrepareScene(Scene& scene, LoaderCache& cache);

  static void LoadEntity(Scene& scene, const tinygltf::Model& model,
                         flecs::entity parent, int gltf_node_idx,
                         LoaderCache& cache);

  static void LoadObject(Scene& scene, flecs::entity entity,
                         const tinygltf::Model& model,
                         const tinygltf::Node& node, LoaderCache& cache);

  static void AddTransformComp(flecs::entity entity,
                               const tinygltf::Node& node);

  static void AddMeshComp(Scene& scene, flecs::entity entity,
                          const tinygltf::Model& model, int mesh_idx,
                          LoaderCache& cache);

  // helpers
  static int GetLightIdx(const tinygltf::Node& node, const LoaderCache& cache);
  static int GetAccessorIdx(const tinygltf::Primitive& primitive,
                            const std::string& attr_name, bool req);

  template <typename T>
  static const T* GetVertexData(const tinygltf::Model& model, int accessor_idx,
                                int expected_type, int expected_comp_type,
                                const std::string& attr_name);
  template <typename T>
  static const T* GetIndexData(const tinygltf::Model& model, int accessor_idx);

  static std::pair<int /*uv_set_idx*/, std::vector<int> /*tex_indices*/>
  GetTextureInfos(const tinygltf::Material& material);
};

}  // namespace npr_scene

#endif  // SCENE_LOADER_H_
