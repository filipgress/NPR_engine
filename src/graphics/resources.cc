#include "resources.h"

namespace npr_graphics {

Resources::Resources(const VulkanContext& context, const CommandPool& cmd_pool,
                     vk::Extent2D extent, uint frame_count)
    : c_{context}, frame_count_{frame_count} {
  frame_props_.extent = extent;
  frame_props_.samples = std::min(vk::SampleCountFlagBits::e4, GetMaxSamples());
  frame_props_.depth_stencil_format = context.FindFormat(
      {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);

  if (frame_props_.samples == vk::SampleCountFlagBits::e1)
    INFO("multisampling is not supported!");

  frame_resources_.resize(frame_count_);

  CreateImages();
  CreateBuffers();

  auto cmd_buff = cmd_pool.BeginSingleTimeCmds();
  {
    CreateDefaultColorTex(cmd_buff);

    CreateSphereMesh(cmd_buff);
    CreateConeMesh(cmd_buff);
  }
  cmd_pool.EndSingleTimeCmds(cmd_buff);
}

void Resources::CreateImages() {
  uint idx{0};
  for (auto& res : frame_resources_) {
    // gpass
    res.albedo_metallic_ms = std::make_unique<Texture>(
        c_, frame_props_.albedo_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "albedo_metallic_ms_" + std::to_string(idx));

    res.emissive_roughness_ms = std::make_unique<Texture>(
        c_, frame_props_.emissive_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "emissive_roughness_ms_" + std::to_string(idx));

    res.position_ms = std::make_unique<Texture>(
        c_, frame_props_.position_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "position_ms" + std::to_string(idx));

    res.normal_ms = std::make_unique<Texture>(
        c_, frame_props_.normal_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "normal_ms" + std::to_string(idx));

    res.coverage_ms = std::make_unique<Image>(
        c_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eInputAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "coverage_ms" + std::to_string(idx));

    res.coverage_res = std::make_unique<Texture>(
        c_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "coverage_res" + std::to_string(idx));

    res.depth_stencil_ms = std::make_unique<Texture>(
        c_, frame_props_.depth_stencil_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
        frame_props_.samples, "depth_stencil_ms" + std::to_string(idx));

    // swap pass
    res.present_color = std::make_unique<Texture>(
        c_, frame_props_.albedo_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "present_color_" + std::to_string(idx));

    idx++;
  }
}

void Resources::CreateBuffers() {
  int idx{0};
  for (auto& res : frame_resources_) {
    res.camera_ubo = std::make_unique<UniformBuffer<CameraUnif>>(
        c_, "camera_unif_buff" + std::to_string(idx));
    res.material_ubo = std::make_unique<DynamicUniformBuffer<MaterialUnif>>(
        c_, MAX_MATERIALS, "material_dynamic_unif_buff" + std::to_string(idx));
    res.instance_buff = std::make_unique<InstanceBuffer>(
        c_, MAX_INSTANCES, "instance_buff" + std::to_string(idx));
    res.light_storage = std::make_unique<StorageBuffer<LightStorage>>(
        c_, "light_storage_buff" + std::to_string(idx));

    idx++;
  }
}

void Resources::CreateDefaultColorTex(vk::CommandBuffer cmd_buff) {
  TextureProps tex_data;
  tex_data.name = "default_texture";
  tex_data.width = 1;
  tex_data.height = 1;
  tex_data.type = TextureType::kColor;

  default_color_tex_ = std::make_unique<Texture>(c_, tex_data);
  default_color_tex_->Write(cmd_buff, {255, 255, 255, 255});
}

vk::SampleCountFlagBits Resources::GetMaxSamples() {
  vk::PhysicalDeviceProperties props = c_.GetProperties();
  vk::SampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;

  if (counts & vk::SampleCountFlagBits::e64)
    return vk::SampleCountFlagBits::e64;
  if (counts & vk::SampleCountFlagBits::e32)
    return vk::SampleCountFlagBits::e32;
  if (counts & vk::SampleCountFlagBits::e16)
    return vk::SampleCountFlagBits::e16;
  if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
  if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
  if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;

  return vk::SampleCountFlagBits::e1;
}

void Resources::CreateSphereMesh(vk::CommandBuffer cmd_buff) {
  const int segments = 16;
  const int rings = 16;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // vertices
  for (int y = 0; y <= rings; ++y) {
    float v = float(y) / float(rings);
    float theta = v * glm::pi<float>();
    float sin_theta = std::sin(theta);
    float cos_theta = std::cos(theta);

    for (int x = 0; x <= segments; ++x) {
      float u = float(x) / float(segments);
      float phi = u * glm::two_pi<float>();
      float sin_phi = std::sin(phi);
      float cos_phi = std::cos(phi);

      glm::vec3 pos(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);
      glm::vec3 normal = glm::normalize(pos);

      vertices.push_back({pos, {}, normal, {}});
    }
  }

  // indices
  for (int y = 0; y < rings; ++y) {
    for (int x = 0; x < segments; ++x) {
      int i0 = y * (segments + 1) + x;
      int i1 = i0 + 1;
      int i2 = i0 + (segments + 1);
      int i3 = i2 + 1;

      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i1);

      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  sphere_mesh_ = std::make_unique<Mesh>(
      VertexBuffer{c_, cmd_buff, vertices, "sphere_mesh_vbo"},
      IndexBuffer{c_, cmd_buff, indices, "sphere_mesh_ibo"});
}

void Resources::CreateConeMesh(vk::CommandBuffer cmd_buff) {
  const int segments = 32;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // tip vertex
  vertices.push_back({glm::vec3(0, 0, 0), {}, glm::vec3(0, 0, 1), {}});
  int tip_idx = 0;

  // base center vertex
  vertices.push_back({glm::vec3(0, 0, -1), {}, glm::vec3(0, 0, -1), {}});
  int base_center_idx = 1;

  // base ring vertices
  for (int i = 0; i < segments; ++i) {
    float angle = (float(i) / segments) * glm::two_pi<float>();
    float x = std::cos(angle);
    float y = std::sin(angle);
    vertices.push_back(
        {glm::vec3(x, y, -1), {}, glm::normalize(glm::vec3(x, y, 0.5f)), {}});
  }

  // side faces (tip to base, CCW)
  for (int i = 0; i < segments; ++i) {
    int v0 = tip_idx;
    int v1 = 2 + i;
    int v2 = 2 + ((i + 1) % segments);

    indices.push_back(v0);
    indices.push_back(v1);
    indices.push_back(v2);
  }

  // base faces (base center to base ring, CCW)
  for (int i = 0; i < segments; ++i) {
    int v0 = base_center_idx;
    int v1 = 2 + ((i + 1) % segments);
    int v2 = 2 + i;

    indices.push_back(v0);
    indices.push_back(v1);
    indices.push_back(v2);
  }

  cone_mesh_ = std::make_unique<Mesh>(
      VertexBuffer{c_, cmd_buff, vertices, "cone_mesh_vbo"},
      IndexBuffer{c_, cmd_buff, indices, "cone_mesh_ibo"});
}

}  // namespace npr_graphics
