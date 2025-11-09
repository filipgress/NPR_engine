#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "scene_loader.h"
#include "components.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace npr_scene {
using namespace npr_graphics;

void SceneLoader::LoadAsync(const npr_graphics::Renderer& renderer,
                            Scene& scene, npr_core::TaskManager& tasks,
                            std::function<void()> on_loaded,
                            const std::string& filepath,
                            const std::string& scene_name) {
  if (scene.handle_.valid()) return;
  scene.handle_ =
      std::async(std::launch::async, &SceneLoader::LoadScene,
                 std::cref(renderer), std::ref(scene), filepath, scene_name);

  tasks.Add([&scene, &tasks, on_loaded]() {
    if (scene.IsLoading()) return false;
    if (!scene.IsValid()) return true;
    scene.InitGPU(tasks, on_loaded);
    return true;
  });
}

void SceneLoader::Load(const Renderer& renderer, Scene& scene,
                       const std::string& filepath,
                       const std::string& scene_name) {
  scene.WaitForAsync();

  scene.valid_ = LoadScene(renderer, scene, filepath, scene_name);
  scene.gpu_init_ = false;
}

bool SceneLoader::LoadScene(const Renderer& renderer, Scene& scene,
                            const std::string& filepath,
                            const std::string& scene_name) {
  try {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string err, warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

    if (!warn.empty()) throw std::runtime_error("gltf warn: " + warn);
    if (!err.empty()) throw std::runtime_error("gltf err: " + err);
    if (!ret) throw std::runtime_error("gltf failed to parse file: " + err);

    scene.Prepare(renderer.GetContext(), filepath, scene_name);
    INFO("loading scene: ", scene.filename_, "(", scene.scene_name_, ")");

    LoaderCache cache;
    cache.light_supp = model.extensions.contains("KHR_lights_punctual");
    if (!cache.light_supp) INFO("scene doesn't support lights");

    {
      scene.gpu_res_->cmd_buff.begin(
          {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

      int scene_idx = GetSceneIdx(model, scene_name);
      for (int node_idx : model.scenes[scene_idx].nodes)
        LoadEntity(scene, flecs::entity::null(), model, node_idx, cache);

      scene.gpu_res_->UpdateTextureDescriptors(
          renderer.GetResources().GetDefaultColorTex());
      scene.gpu_res_->cmd_buff.end();
    }

  } catch (const std::runtime_error& e) {
    ERR("failed to load scene: ", e.what());
    return false;
  }

  INFO("scene loaded: ", scene.filename_, "(", scene.scene_name_, ")");
  return true;
}

void SceneLoader::LoadEntity(Scene& scene, flecs::entity parent_ent,
                             const tinygltf::Model& model, int node_idx,
                             LoaderCache& cache) {
  std::stack<std::pair<flecs::entity, int>> s;
  s.push({parent_ent, node_idx});

  while (!s.empty()) {
    auto [parent_ent, node_idx] = s.top();
    s.pop();

    if (cache.nodes.contains(node_idx)) continue;
    cache.nodes.insert(node_idx);

    const auto& node = model.nodes[node_idx];

    std::string entity_name =
        scene.GetSceneName() + "_node_" + std::to_string(node_idx) + node.name;

    flecs::entity child_ent =
        scene.world_.entities_.entity(entity_name.c_str());
    if (parent_ent.is_valid()) child_ent.child_of(parent_ent);

    if (node.camera != -1)
      LoadCamera(child_ent, model, node);
    else if (int light_idx = GetLightIdx(node, cache); light_idx == -1)
      LoadObject(scene, child_ent, model, node, cache);
    else {
    }  // child_ent.add<LightTag>();

    for (int glfw_child_node_idx : node.children)
      s.push({child_ent, glfw_child_node_idx});
  }
}

void SceneLoader::LoadObject(Scene& scene, flecs::entity node_ent,
                             const tinygltf::Model& model,
                             const tinygltf::Node& node, LoaderCache& cache) {
  node_ent.add<ObjectTag>();
  AddTransformComp(node_ent, node);

  if (node.mesh == -1) return;
  const auto& mesh = model.meshes[node.mesh];

  int prim_idx{0};
  for (const auto& prim : mesh.primitives) {
    if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
      INFO("[warn]: skipping unsupported geometry primitive");
      continue;
    }
    if (prim.material == -1) {
      INFO("[warn]: skipping primitive without material");
      continue;
    }

    std::string prim_name = node.name + "_prim_" + std::to_string(prim_idx);
    flecs::entity prim_ent = scene.world_.entities_.entity(prim_name.c_str());

    prim_ent.child_of(node_ent);
    prim_ent.add<PrimitiveTag>();

    AddMeshComp(scene, prim_ent, model, node, prim_idx, cache);
    AddMaterialComp(scene, prim_ent, model, node, prim_idx, cache);

    prim_idx++;
  }
}

void SceneLoader::LoadCamera(flecs::entity node_ent,
                             const tinygltf::Model& model,
                             const tinygltf::Node& node) {
  node_ent.add<CameraTag>();
  AddTransformComp(node_ent, node);

  const auto& camera = model.cameras[node.camera];

  if (camera.type == "perspective") {
    PerspectiveComp comp;
    comp.aspect = camera.perspective.aspectRatio;
    comp.fov = camera.perspective.yfov;
    comp.far = camera.perspective.zfar;
    comp.near = camera.perspective.znear;
    node_ent.set<PerspectiveComp>(comp);

  } else if (camera.type == "orthographic") {
    OrthographicComp comp;
    comp.xmag = camera.orthographic.xmag;
    comp.ymag = camera.orthographic.ymag;
    comp.far = camera.orthographic.zfar;
    comp.near = camera.orthographic.znear;
    node_ent.set<OrthographicComp>(comp);

  } else {
    INFO("[warn]: skipping unsupported camera type: " + camera.type);
  }
}

void SceneLoader::LoadLight(flecs::entity node_ent,
                            const tinygltf::Model& model,
                            const tinygltf::Node& node, int light_idx) {
  AddTransformComp(node_ent, node);

  const auto& gltf_light = model.lights[light_idx];

  LightComp light;
  light.color = {gltf_light.color[0], gltf_light.color[1], gltf_light.color[2]};
  light.intensity = gltf_light.intensity;
  node_ent.set<LightComp>(light);

  if (gltf_light.type == "directional") {
    node_ent.add<DirLightTag>();

  } else if (gltf_light.type == "point") {
    node_ent.add<PointLightTag>();

    RangeComp range_comp;
    range_comp.range = gltf_light.range;
    node_ent.set<RangeComp>(range_comp);

  } else if (gltf_light.type == "spot") {
    node_ent.add<SpotLightTag>();

    RangeComp range_comp;
    range_comp.range = gltf_light.range;
    node_ent.set<RangeComp>(range_comp);

    SpotComp spot_comp;
    spot_comp.inner_cone_angle = gltf_light.spot.innerConeAngle;
    spot_comp.outer_cone_angle = gltf_light.spot.outerConeAngle;
    node_ent.set<SpotComp>(spot_comp);

  } else {
    INFO("[warn]: skipping unsupported light type: " + gltf_light.type);
    return;
  }
}

void SceneLoader::AddTransformComp(flecs::entity ent,
                                   const tinygltf::Node& node) {
  TransformComp transform_comp;

  if (!node.matrix.empty()) {
    glm::mat4 mat;
    for (int i = 0; i < 16; i++) *(&mat[0][0] + i) = node.matrix[i];

    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(mat, transform_comp.scale, transform_comp.rot,
                   transform_comp.pos, skew, perspective);
  } else {
    if (!node.translation.empty())
      transform_comp.pos = glm::vec3(node.translation[0], node.translation[1],
                                     node.translation[2]);
    if (!node.scale.empty())
      transform_comp.scale =
          glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
    if (!node.rotation.empty())
      transform_comp.rot = glm::quat(node.rotation[3], node.rotation[0],
                                     node.rotation[1], node.rotation[2]);
  }

  ent.set<TransformComp>(transform_comp);
}

void SceneLoader::AddMeshComp(Scene& scene, flecs::entity ent,
                              const tinygltf::Model& model,
                              const tinygltf::Node& node, const int prim_idx,
                              LoaderCache& cache) {
  if (cache.meshes.contains({node.mesh, prim_idx})) {
    ent.set<MeshComp>(cache.meshes.at({node.mesh, prim_idx}));
    return;
  }

  const auto& mesh = model.meshes[node.mesh];
  const auto& prim = mesh.primitives[prim_idx];

  MeshComp mesh_comp;

  {  // load vertices
    int pos_acc_idx = GetAccessorIdx(prim, "POSITION", true);
    int norm_acc_idx = GetAccessorIdx(prim, "NORMAL", true);
    int tan_acc_idx = GetAccessorIdx(prim, "TANGENT", false);
    int uv_acc_idx = -1;

    int uv_set_idx = GetTexUvSetIdx(model.materials[prim.material]);
    if (uv_set_idx != -1)
      uv_acc_idx =
          GetAccessorIdx(prim, "TEXCOORD_" + std::to_string(uv_set_idx), false);

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
    std::vector<Vertex> vertices(vert_count);

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

    scene.gpu_res_->vbos.emplace_back(scene.gpu_res_->context,
                                      scene.gpu_res_->cmd_buff, vertices,
                                      "vbo_" + std::string(ent.name().c_str()));
    mesh_comp.vbo_idx = scene.gpu_res_->vbos.size() - 1;
  }

  if (prim.indices != -1) {  // load indices
    const auto& acc = model.accessors[prim.indices];
    std::vector<uint32_t> indices(acc.count);

    switch (acc.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const auto* idx_data = GetIndexData<uint8_t>(model, prim.indices);
        for (size_t i = 0; i < acc.count; ++i)
          indices[i] = static_cast<uint32_t>(idx_data[i]);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const auto* idx_data = GetIndexData<uint16_t>(model, prim.indices);
        for (size_t i = 0; i < acc.count; ++i)
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
                                 std::to_string(acc.componentType));
    }

    scene.gpu_res_->ibos.emplace_back(scene.gpu_res_->context,
                                      scene.gpu_res_->cmd_buff, indices,
                                      "ibo_" + std::string(ent.name().c_str()));
    mesh_comp.ibo_idx = scene.gpu_res_->ibos.size() - 1;
  }

  ent.set<MeshComp>(mesh_comp);
  cache.meshes[{node.mesh, prim_idx}] = mesh_comp;
}

void SceneLoader::AddMaterialComp(Scene& scene, flecs::entity ent,
                                  const tinygltf::Model& model,
                                  const tinygltf::Node& node,
                                  const int prim_idx, LoaderCache& cache) {
  const auto& mesh = model.meshes[node.mesh];
  const auto& prim = mesh.primitives[prim_idx];

  if (cache.materials.contains(prim.material)) {
    ent.set<MaterialComp>(cache.materials.at(prim.material));
    return;
  }

  const auto& material = model.materials[prim.material];

  MaterialComp material_comp;

  // attributes
  material_comp.double_sided = material.doubleSided;
  material_comp.is_opaque = (material.alphaMode == "OPAQUE");
  material_comp.is_mask = (material.alphaMode == "MASK");
  material_comp.alpha_cutoff = material.alphaCutoff;

  // factors
  if (const auto& bcf = material.pbrMetallicRoughness.baseColorFactor;
      !bcf.empty())
    material_comp.color_factor = glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3]);
  if (const auto& ef = material.emissiveFactor; !ef.empty())
    material_comp.emissive_factor = glm::vec3(ef[0], ef[1], ef[2]);
  material_comp.metallic_factor = material.pbrMetallicRoughness.metallicFactor;
  material_comp.roughness_factor =
      material.pbrMetallicRoughness.roughnessFactor;

  //  textures
  material_comp.color_map_idx =
      LoadTexture(scene, model, material, TextureType::kColor);
  material_comp.normal_map_idx =
      LoadTexture(scene, model, material, TextureType::kNormal);
  material_comp.metallic_roughness_map_idx =
      LoadTexture(scene, model, material, TextureType::kMetallicRoughness);
  material_comp.emissive_map_idx =
      LoadTexture(scene, model, material, TextureType::kEmissive);

  ent.set<MaterialComp>(material_comp);
  cache.materials[prim.material] = material_comp;
}

int SceneLoader::LoadTexture(Scene& scene, const tinygltf::Model& model,
                             const tinygltf::Material& material,
                             const TextureType tex_type) {
  int tex_idx = GetTexIdx(material, tex_type);
  if (tex_idx == -1) return -1;

  const auto& tex_info = Texture::GetTypeInfo(tex_type);
  const auto& texture = model.textures[tex_idx];
  const auto& image = model.images[texture.source];

  int component = image.component;
  const std::vector<unsigned char>* data = &image.image;

  std::vector<unsigned char> converted;
  if (tex_type == TextureType::kMetallicRoughness && component > 2) {
    // convert to use two components
    converted.reserve(image.width * image.height * 2);

    for (size_t i = 0; i < image.image.size(); i += component) {
      converted.push_back(image.image[i + 2]);  // metallic from B
      converted.push_back(image.image[i + 1]);  // roughness from G
    }

    data = &converted;
    component = 2;

  } else if (component == 3) {
    // pad RGB to RGBA with 255 for alpha
    converted.reserve(image.width * image.height * tex_info.component);

    for (size_t i = 0; i < image.image.size(); i += component) {
      converted.push_back(image.image[i]);
      converted.push_back(image.image[i + 1]);
      converted.push_back(image.image[i + 2]);
      converted.push_back(255);
    }

    data = &converted;
    component = 4;
  }

  if (component != tex_info.component ||
      (image.pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
       image.pixel_type != TINYGLTF_COMPONENT_TYPE_BYTE))
    throw std::runtime_error("texture type '" + tex_info.name +
                             "' is in unexpected format");

  TextureProps tex_data;
  tex_data.name = tex_info.name + (image.name.empty() ? "" : "_" + image.name);
  tex_data.type = tex_type;
  tex_data.height = image.height;
  tex_data.width = image.width;

  Texture tex{scene.gpu_res_->context, tex_data, LoadSampler(model, texture)};
  tex.Write(scene.gpu_res_->cmd_buff, *data);

  scene.gpu_res_->textures.push_back(std::move(tex));
  int idx = scene.gpu_res_->textures.size() - 1;

  return idx;
}

SamplerProps SceneLoader::LoadSampler(const tinygltf::Model& model,
                                      const tinygltf::Texture& texture) {
  SamplerProps props;
  if (texture.sampler == -1) return props;

  const auto& sampler = model.samplers[texture.sampler];

  props.mag_filter = sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST
                         ? vk::Filter::eNearest
                         : vk::Filter::eLinear;

  switch (sampler.minFilter) {
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
      props.min_filter = vk::Filter::eNearest;
      props.mipmap_mode = vk::SamplerMipmapMode::eNearest;
      break;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
      props.min_filter = vk::Filter::eNearest;
      props.mipmap_mode = vk::SamplerMipmapMode::eNearest;
      break;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
      props.min_filter = vk::Filter::eNearest;
      props.mipmap_mode = vk::SamplerMipmapMode::eLinear;
      break;
    case TINYGLTF_TEXTURE_FILTER_LINEAR:
      props.min_filter = vk::Filter::eLinear;
      props.mipmap_mode = vk::SamplerMipmapMode::eNearest;
      break;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
      props.min_filter = vk::Filter::eLinear;
      props.mipmap_mode = vk::SamplerMipmapMode::eNearest;
      break;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
      props.min_filter = vk::Filter::eLinear;
      props.mipmap_mode = vk::SamplerMipmapMode::eLinear;
      break;
    default:
      props.min_filter = vk::Filter::eLinear;
      props.mipmap_mode = vk::SamplerMipmapMode::eLinear;
      break;
  }

  static const std::unordered_map<int, vk::SamplerAddressMode> wrapModeMap = {
      {TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE,
       vk::SamplerAddressMode::eClampToEdge},
      {TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT,
       vk::SamplerAddressMode::eMirroredRepeat},
      {TINYGLTF_TEXTURE_WRAP_REPEAT, vk::SamplerAddressMode::eRepeat}};

  props.address_mode_U = wrapModeMap.at(sampler.wrapS);
  props.address_mode_V = wrapModeMap.at(sampler.wrapT);

  return props;
}

/*
 * Helpers
 */

int SceneLoader::GetSceneIdx(const tinygltf::Model& model,
                             const std::string& scene_name) {
  int scene_idx{-1};

  if (scene_name.empty()) {
    scene_idx = model.defaultScene;
  } else {
    for (int i = 0; i < static_cast<int>(model.scenes.size()); ++i) {
      if (model.scenes[i].name != scene_name) continue;

      scene_idx = i;
      break;
    }
  }

  if (scene_idx == -1)
    throw std::runtime_error("scene with name '" +
                             (scene_name.empty() ? "default" : scene_name) +
                             "' not found");
  return scene_idx;
}

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
    throw std::runtime_error("mesh is missing required attribute: " +
                             attr_name);
  return acc_idx;
}

template <typename T>
const T* SceneLoader::GetVertexData(const tinygltf::Model& model, int acc_idx,
                                    int expected_type, int expected_comp_type,
                                    const std::string& attr_name) {
  if (acc_idx == -1) return nullptr;

  const auto& acc = model.accessors[acc_idx];
  if (acc.type != expected_type || acc.componentType != expected_comp_type)
    throw std::runtime_error("mesh attribute '" + attr_name +
                             "' is in incorrect format");

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

int SceneLoader::GetTexUvSetIdx(const tinygltf::Material& material) {
  std::vector<int> tex_uv_indices{
      material.pbrMetallicRoughness.baseColorTexture.texCoord,
      material.normalTexture.texCoord,
      material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord,
      material.emissiveTexture.texCoord};

  std::set<int> unique_uv_set_indices;
  for (const auto& tex_uv_idx : tex_uv_indices) {
    if (tex_uv_idx == -1) continue;
    unique_uv_set_indices.emplace(tex_uv_idx);
  }

  if (unique_uv_set_indices.size() > 1)
    throw std::runtime_error(
        "materials with multiple uv sets are not supported");

  return unique_uv_set_indices.empty() ? -1 : *unique_uv_set_indices.begin();
}

int SceneLoader::GetTexIdx(const tinygltf::Material& material,
                           TextureType tex_type) {
  switch (tex_type) {
    case TextureType::kColor:
      return material.pbrMetallicRoughness.baseColorTexture.index;
    case TextureType::kNormal:
      return material.normalTexture.index;
    case TextureType::kMetallicRoughness:
      return material.pbrMetallicRoughness.metallicRoughnessTexture.index;
    case TextureType::kEmissive:
      return material.emissiveTexture.index;
    default:
      throw std::runtime_error("unsupported texture type");
  }
}

}  // namespace npr_scene
