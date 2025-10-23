#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "scene_loader.h"
#include "components.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace npr_scene {

void SceneLoader::LoadAsync(const npr_graphics::VulkanContext& context,
                            Scene& scene, const std::string& scene_file) {
  if (scene.handle_.valid()) return;
  scene.handle_ = std::async(std::launch::async, &SceneLoader::LoadScene,
                             std::cref(context), std::ref(scene), scene_file);
}

void SceneLoader::Load(const npr_graphics::VulkanContext& context, Scene& scene,
                       const std::string& scene_file) {
  scene.WaitForAsync();
  scene.valid_ = LoadScene(context, scene, scene_file);
}

bool SceneLoader::LoadScene(const npr_graphics::VulkanContext& context,
                            Scene& scene, const std::string& scene_file) {
  LoaderCache cache{context, scene_file};
  INFO("loading scene: ", cache.name);

  try {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string err, warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, scene_file);

    if (!warn.empty()) throw std::runtime_error("gltf warn: " + warn);
    if (!err.empty()) throw std::runtime_error("gltf err: " + err);
    if (!ret) throw std::runtime_error("gltf failed to parse file: " + err);

    cache.light_supp = model.extensions.contains("KHR_lights_punctual");
    if (!cache.light_supp) INFO("scene doesn't support lights");

    PrepareScene(scene, cache);
    cache.cmd_buff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    for (int node_idx : model.scenes[model.defaultScene].nodes)
      LoadEntity(scene, model, flecs::entity::null(), node_idx, cache);

    cache.cmd_buff.end();

    INFO("scene loaded: ", cache.name);
    return true;

  } catch (const std::runtime_error& e) {
    ERR("failed to load scene: ", e.what());
    return false;
  }
}

void SceneLoader::PrepareScene(Scene& scene, LoaderCache& cache) {
  scene.Clear();
  scene.name_ = cache.name;

  if (scene.gpu_resources_.cmd_pool) {
    cache.cmd_buff = scene.gpu_resources_.cmd_pool->GetCmdBuff();
    cache.cmd_buff.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
  } else {
    const auto& q_families = cache.context.GetQFamilies();
    uint32_t q_idx = q_families.transfer_i.has_value()
                         ? q_families.transfer_i.value()
                         : q_families.graphics_i.value();

    scene.gpu_resources_.cmd_pool = std::make_unique<npr_graphics::CommandPool>(
        cache.context, 1,
        vk::CommandPoolCreateFlagBits::eTransient |
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        q_idx, "cmd_pool_" + cache.name);

    cache.cmd_buff = scene.gpu_resources_.cmd_pool->GetCmdBuff();
  }

  // if (!scene.gpu_resources_.desc_pool) {
  //   scene.gpu_resources_.desc_pool =
  //       std::make_unique<npr_graphics::DescriptorPool>();
  // }
}

void SceneLoader::LoadEntity(Scene& scene, const tinygltf::Model& model,
                             flecs::entity parent, int gltf_node_idx,
                             LoaderCache& cache) {
  std::stack<std::pair<flecs::entity, int>> s;
  s.push({parent, gltf_node_idx});

  while (!s.empty()) {
    auto [parent_ent, gltf_node_idx] = s.top();
    s.pop();

    const auto& gltf_node = model.nodes[gltf_node_idx];

    flecs::entity child_ent = scene.entities_.entity(gltf_node.name.c_str());
    if (parent_ent.is_valid()) child_ent.child_of(parent_ent);

    if (gltf_node.camera) {
      child_ent.add<CameraTag>();

    } else if (int light_idx = GetLightIdx(gltf_node, cache); light_idx == -1) {
      child_ent.add<ObjectTag>();
      LoadObject(scene, child_ent, model, gltf_node, cache);

    } else {
      child_ent.add<LightTag>();
    }

    for (int glfw_node_child_idx : gltf_node.children)
      s.push({child_ent, glfw_node_child_idx});
  }
}

void SceneLoader::LoadObject(Scene& scene, flecs::entity entity,
                             const tinygltf::Model& model,
                             const tinygltf::Node& node, LoaderCache& cache) {
  AddTransformComp(entity, node);
  AddMeshComp(scene, entity, model, node.mesh, cache);
  // AddMaterial(entity, model, node.mesh, cache);
}

void SceneLoader::AddTransformComp(flecs::entity entity,
                                   const tinygltf::Node& node) {
  TransformComp transform;

  if (!node.matrix.empty()) {
    glm::mat4 mat;
    for (int i = 0; i < 16; i++) *(&mat[0][0] + i) = node.matrix[i];

    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform.local_mat, transform.scale, transform.rot,
                   transform.pos, skew, perspective);
  } else {
    if (!node.translation.empty())
      transform.pos = glm::vec3(node.translation[0], node.translation[1],
                                node.translation[2]);
    if (!node.scale.empty())
      transform.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
    if (!node.rotation.empty())
      transform.rot = glm::quat(node.rotation[3], node.rotation[0],
                                node.rotation[1], node.rotation[2]);
  }

  entity.set<TransformComp>(transform);
}

void SceneLoader::AddMeshComp(Scene& scene, flecs::entity entity,
                              const tinygltf::Model& model, int mesh_idx,
                              LoaderCache& cache) {
  if (mesh_idx == -1) return;
  if (cache.meshes.contains(mesh_idx)) {
    entity.set<MeshComp>(cache.meshes.at(mesh_idx));
    return;
  }

  const auto& gltf_mesh = model.meshes[mesh_idx];
  const auto& prim = gltf_mesh.primitives[0];

  if (prim.mode != TINYGLTF_MODE_TRIANGLES)
    throw std::runtime_error("primitive mode \"" + std::to_string(prim.mode) +
                             "\" is not supported");

  MeshComp mesh;

  {  // load vertices
    int pos_acc_idx = GetAccessorIdx(prim, "POSITION", true);
    int norm_acc_idx = GetAccessorIdx(prim, "NORMAL", true);
    int tan_acc_idx = GetAccessorIdx(prim, "TANGENT", false);
    int uv_acc_idx = -1;

    if (prim.material) {
      const auto& [uv_set_idx, _] =
          GetTextureInfos(model.materials[prim.material]);
      if (uv_set_idx != -1)
        uv_acc_idx = GetAccessorIdx(
            prim, "TEXCOORD_" + std::to_string(uv_set_idx), false);
    }

    const auto* pos_data =
        GetVertexData<float[3]>(model, pos_acc_idx, TINYGLTF_TYPE_VEC3,
                                TINYGLTF_COMPONENT_TYPE_FLOAT, "POSITION");
    const auto* norm_data =
        GetVertexData<float[3]>(model, norm_acc_idx, TINYGLTF_TYPE_VEC3,
                                TINYGLTF_COMPONENT_TYPE_FLOAT, "NORMAL");
    const auto* tangent_data =
        GetVertexData<float[4]>(model, tan_acc_idx, TINYGLTF_TYPE_VEC4,
                                TINYGLTF_COMPONENT_TYPE_FLOAT, "TANGENT");
    const auto* uv_data =
        GetVertexData<float[2]>(model, uv_acc_idx, TINYGLTF_TYPE_VEC2,
                                TINYGLTF_COMPONENT_TYPE_FLOAT, "UV");

    size_t vert_count = model.accessors[pos_acc_idx].count;
    std::vector<npr_graphics::Vertex> vertices(vert_count);

    for (size_t i = 0; i < vert_count; ++i) {
      vertices[i].pos =
          glm::vec3(pos_data[i][0], pos_data[i][1], pos_data[i][2]);
      vertices[i].normal =
          glm::vec3(norm_data[i][0], norm_data[i][1], norm_data[i][2]);
      if (uv_data) vertices[i].uv = glm::vec2(uv_data[i][0], uv_data[i][1]);
      if (tangent_data)
        vertices[i].tangent = glm::vec4(tangent_data[i][0], tangent_data[i][1],
                                        tangent_data[i][2], tangent_data[i][3]);
    }

    scene.gpu_resources_.vbos.emplace_back(
        cache.context, cache.cmd_buff, vertices,
        "vbo_" + std::string(entity.name().c_str()));
    mesh.vbo_idx = scene.gpu_resources_.vbos.size() - 1;
  }

  if (prim.indices != -1) {  // load indices
    const auto& accessor = model.accessors[prim.indices];
    std::vector<uint32_t> indices(accessor.count);

    switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const auto* idx_data = GetIndexData<uint8_t>(model, prim.indices);
        for (size_t i = 0; i < accessor.count; ++i)
          indices[i] = static_cast<uint32_t>(idx_data[i]);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const auto* idx_data = GetIndexData<uint16_t>(model, prim.indices);
        for (size_t i = 0; i < accessor.count; ++i)
          indices[i] = static_cast<uint32_t>(idx_data[i]);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        const auto* idx_data = GetIndexData<uint32_t>(model, prim.indices);
        memcpy(indices.data(), idx_data, indices.size() * sizeof(uint32_t));
        break;
      }
      default:
        throw std::runtime_error("unsupported index buffer type: " +
                                 std::to_string(accessor.componentType));
    }

    scene.gpu_resources_.ibos.emplace_back(
        cache.context, cache.cmd_buff, indices,
        "ibo_" + std::string(entity.name().c_str()));
    mesh.ibo_idx = scene.gpu_resources_.ibos.size() - 1;
  }

  entity.set<MeshComp>(mesh);
  cache.meshes.emplace(mesh_idx, mesh);
}

/*
 * Helpers
 */

int SceneLoader::GetLightIdx(const tinygltf::Node& node,
                             const LoaderCache& cache) {
  if (!cache.light_supp || !node.extensions.contains("KHR_lights_punctual"))
    return -1;
  return node.extensions.at("KHR_lights_punctual").Get("light").Get<int>();
}

int SceneLoader::GetAccessorIdx(const tinygltf::Primitive& primitive,
                                const std::string& attr_name, bool req) {
  int acc_idx{-1};
  if (primitive.attributes.contains(attr_name))
    acc_idx = primitive.attributes.at(attr_name);

  if (acc_idx == -1 && req)
    throw std::runtime_error("mesh is missing required attribute \"" +
                             attr_name + "\"");
  return acc_idx;
}

template <typename T>
const T* SceneLoader::GetVertexData(const tinygltf::Model& model, int acc_idx,
                                    int expected_type, int expected_comp_type,
                                    const std::string& attr_name) {
  if (acc_idx == -1) return nullptr;

  const auto& acc = model.accessors[acc_idx];
  if (acc.type != expected_type || acc.componentType != expected_comp_type)
    throw std::runtime_error("mesh attribute \"" + attr_name +
                             "\" is in incorrect format");

  const auto& view = model.bufferViews[acc.bufferView];
  const auto& buffer = model.buffers[view.buffer];

  return reinterpret_cast<const T*>(buffer.data.data() + view.byteOffset +
                                    acc.byteOffset);
}

template <typename T>
const T* SceneLoader::GetIndexData(const tinygltf::Model& model, int acc_idx) {
  const auto& acc = model.accessors[acc_idx];
  const auto& view = model.bufferViews[acc.bufferView];
  const auto& buffer = model.buffers[view.buffer];

  return reinterpret_cast<const T*>(buffer.data.data() + view.byteOffset +
                                    acc.byteOffset);
}

std::pair<int /*uv_set_idx*/, std::vector<int> /*tex_indices*/>
SceneLoader::GetTextureInfos(const tinygltf::Material& material) {
  std::vector<int> tex_indices{
      material.pbrMetallicRoughness.baseColorTexture.index,
      material.normalTexture.index,
      material.pbrMetallicRoughness.metallicRoughnessTexture.index,
      material.emissiveTexture.index};

  std::vector<int> tex_uv_indices{
      material.pbrMetallicRoughness.baseColorTexture.texCoord,
      material.normalTexture.texCoord,
      material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord,
      material.emissiveTexture.texCoord};

  std::set<int> unique_uv_set_indices;
  for (size_t i = 0; i < tex_indices.size(); i++) {
    if (tex_indices[i] == -1 || tex_uv_indices[i] == -1) continue;
    unique_uv_set_indices.insert(tex_uv_indices[i]);
  }

  if (unique_uv_set_indices.size() > 1)
    throw std::runtime_error(
        "materials with multiple uv sets are not supported");

  int uv_set_idx =
      unique_uv_set_indices.empty() ? -1 : *unique_uv_set_indices.begin();
  return {uv_set_idx, tex_indices};
}

}  // namespace npr_scene
