#include "resources.h"
#include <random>
#include <stb_image.h>

namespace npr_graphics {

Resources::Resources(const Context& ctx, const CommandPool& cmd_pool,
                     const RenderSettings& settings, uint frame_count)
    : ctx_{ctx}, frame_count_{frame_count} {
  frame_props_.extent = settings.target_size;
  frame_props_.abuff_avg_nodes = settings.abuff_avg_nodes;

  frame_props_.samples = std::min(vk::SampleCountFlagBits::e4, GetMaxSamples());
  frame_props_.ds_format = ctx_.FindFormat(
      {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);

  if (frame_props_.samples == vk::SampleCountFlagBits::e1)
    INFO("multisampling is not supported!");

  frame_resrc_.resize(frame_count_);

  CreateImages();
  CreateBuffers();
  CreateABuffers();

  CreateSSAOKernel();
  CreatePoisKernels();

  blur_ssao_pc_ = GenGausKernel(5);
  blur_bloom_pc_ = GenGausKernel(8);

  auto cmd_buff = cmd_pool.BeginSingleTimeCmds();
  {
    CreateDefaultColorTex(cmd_buff);
    CreateWhiteNoiseTex(cmd_buff);
    CreateAONoiseTex(cmd_buff);
    LoadBlueTextures(cmd_buff);

    CreateSphereMesh(cmd_buff);
    CreateConeMesh(cmd_buff);
  }
  cmd_pool.EndSingleTimeCmds(cmd_buff);
}

// === create frame resources ===

void Resources::CreateImages() {
  uint idx{0};
  for (auto& resrc : frame_resrc_) {
    // gbuff pass
    resrc.albedo_metallic_ms = std::make_unique<Texture>(
        ctx_, frame_props_.albedo_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "albedo_metallic_ms_" + std::to_string(idx));

    resrc.emissive_roughness_ms = std::make_unique<Texture>(
        ctx_, frame_props_.emissive_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "emissive_roughness_ms_" + std::to_string(idx));

    resrc.position_ms = std::make_unique<Texture>(
        ctx_, frame_props_.position_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "position_ms_" + std::to_string(idx));

    resrc.normal_ms = std::make_unique<Texture>(
        ctx_, frame_props_.normal_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, frame_props_.samples,
        "normal_ms_" + std::to_string(idx));

    resrc.coverage_ms = std::make_unique<Image>(
        ctx_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eInputAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "coverage_ms_" + std::to_string(idx));

    resrc.coverage_res = std::make_unique<Texture>(
        ctx_, frame_props_.coverage_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "coverage_res_" + std::to_string(idx));

    resrc.ds_ms = std::make_unique<Texture>(
        ctx_, frame_props_.ds_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eDepth, frame_props_.samples,
        "depth_stencil_ms_" + std::to_string(idx));

    // ao pass
    resrc.ao_ms = std::make_unique<Image>(
        ctx_, frame_props_.ao_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "ao_ms_" + std::to_string(idx));

    resrc.ao_res = std::make_unique<Texture>(
        ctx_, frame_props_.ao_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "ao_res_" + std::to_string(idx));

    resrc.ao_temp = std::make_unique<Texture>(
        ctx_, frame_props_.ao_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "ao_temp_" + std::to_string(idx));

    // wboit pass
    resrc.acc_color_ms = std::make_unique<Image>(
        ctx_, frame_props_.acc_color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "acc_color_ms_" + std::to_string(idx));

    resrc.acc_color_res = std::make_unique<Image>(
        ctx_, frame_props_.acc_color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eInputAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        vk::SampleCountFlagBits::e1, 1, "acc_color_res_" + std::to_string(idx));

    resrc.acc_weight_ms = std::make_unique<Image>(
        ctx_, frame_props_.acc_weight_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "acc_weight_ms_" + std::to_string(idx));

    resrc.acc_weight_res = std::make_unique<Image>(
        ctx_, frame_props_.acc_weight_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eInputAttachment,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        vk::SampleCountFlagBits::e1, 1,
        "acc_weight_res_" + std::to_string(idx));

    // bloom
    resrc.bright_color = std::make_unique<Texture>(
        ctx_, frame_props_.color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "bright_color_" + std::to_string(idx));

    resrc.bright_temp = std::make_unique<Texture>(
        ctx_, frame_props_.color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "bright_temp_" + std::to_string(idx));

    // dof
    resrc.coc_map = std::make_unique<Texture>(
        ctx_, frame_props_.coc_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "coc_map_" + std::to_string(idx));

    // color targets
    resrc.color_ms = std::make_unique<Image>(
        ctx_, frame_props_.color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageAspectFlagBits::eColor, vk::SharingMode::eExclusive,
        frame_props_.samples, 1, "color_ms_" + std::to_string(idx));

    resrc.color_res = std::make_unique<Texture>(
        ctx_, frame_props_.color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "color_res_" + std::to_string(idx));

    resrc.present_color = std::make_unique<Texture>(
        ctx_, frame_props_.color_format, frame_props_.extent,
        vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
        "present_color_" + std::to_string(idx));

    idx++;
  }
}

void Resources::CreateABuffers() {
  auto total_samples = frame_props_.extent.width * frame_props_.extent.height *
                       static_cast<uint32_t>(frame_props_.samples);
  frame_props_.abuff_max_nodes = total_samples * frame_props_.abuff_avg_nodes;

  for (auto& resrc : frame_resrc_) {
    resrc.abuff_nodes = std::make_unique<StorageBuffer<ABuffNode>>(
        ctx_, frame_props_.abuff_max_nodes, "abuff_nodes");
    resrc.abuff_heads = std::make_unique<StorageBuffer<uint32_t>>(
        ctx_, total_samples, "abuff_heads");
    resrc.abuff_counter =
        std::make_unique<StorageBuffer<uint32_t>>(ctx_, 1, "abuff_counter");
  }
}

void Resources::CreateBuffers() {
  int idx{0};
  for (auto& resrc : frame_resrc_) {
    resrc.instance_buff = std::make_unique<InstanceBuffer>(
        ctx_, kMaxInstances, "instance_buff" + std::to_string(idx));
    resrc.camera_ubo = std::make_unique<UniformBuffer<CameraUnif>>(
        ctx_, "camera_unif_buff" + std::to_string(idx));
    resrc.material_ubo = std::make_unique<DynamicUniformBuffer<MaterialUnif>>(
        ctx_, kMaxMaterials,
        "material_dynamic_unif_buff" + std::to_string(idx));

    resrc.dir_light_ubo = std::make_unique<UniformBuffer<DirLightUnif>>(
        ctx_, "dir_light_unif_buff" + std::to_string(idx));
    resrc.point_light_ubo =
        std::make_unique<DynamicUniformBuffer<PointLightUnif>>(
            ctx_, kMaxPointLights,
            "point_light_dynamic_unif_buff" + std::to_string(idx));
    resrc.spot_light_ubo =
        std::make_unique<DynamicUniformBuffer<SpotLightUnif>>(
            ctx_, kMaxSpotLights,
            "spot_light_dynamic_unif_buff" + std::to_string(idx));

    idx++;
  }
}

// === create kernels ===

void Resources::CreateSSAOKernel() {
  SSAOKernel kernel;

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> rand01(0.0f, 1.0f);

  for (uint32_t i = 0; i < kAOKernelSize; ++i) {
    glm::vec4 sample(rand01(rng) * 2.0f - 1.0f, rand01(rng) * 2.0f - 1.0f,
                     rand01(rng), 0.0f);
    sample = glm::normalize(sample);
    sample *= rand01(rng);

    float scale = float(i) / float(kAOKernelSize - 1);
    scale = glm::mix(0.1f, 1.0f, scale * scale);
    sample *= scale;

    kernel[i] = sample;
  }

  ssao_kernel_ =
      std::make_unique<UniformBuffer<SSAOKernel>>(ctx_, "ao_kernel_unif");
  ssao_kernel_->Write(kernel);
}

void Resources::CreatePoisKernels() {
  auto kernel_32 = GenPoisKernel(32, 128);
  auto kernel_64 = GenPoisKernel(64, 128);
  auto kernel_128 = GenPoisKernel(128, 128);

  pois_kernel_32_ = std::make_unique<UniformBuffer<PoisKernelUnif>>(
      ctx_, "poisson_kernel_32");
  pois_kernel_32_->Write(kernel_32);

  pois_kernel_64_ = std::make_unique<UniformBuffer<PoisKernelUnif>>(
      ctx_, "poisson_kernel_64");
  pois_kernel_64_->Write(kernel_64);

  pois_kernel_128_ = std::make_unique<UniformBuffer<PoisKernelUnif>>(
      ctx_, "poisson_kernel_128");
  pois_kernel_128_->Write(kernel_128);
}

GausKernelPC Resources::GenGausKernel(uint32_t radius) {
  GausKernelPC blur_pc;

  radius = std::clamp(radius, 1u, kMaxGausRadius);
  blur_pc.flags.y = radius;

  float sigma = radius / 3.0f;
  float sigma_sq = 2.0f * sigma * sigma;

  float sum = 0.0f;

  for (uint32_t i = 0; i <= radius; ++i) {
    float w = std::exp(-float(i * i) / sigma_sq);
    blur_pc.weights[i] = w;
    sum += (i == 0) ? w : 2.0f * w;
  }

  for (uint32_t i = 0; i <= radius; ++i) blur_pc.weights[i] /= sum;

  return blur_pc;
}

PoisKernelUnif Resources::GenPoisKernel(uint32_t sample_count,
                                        uint32_t max_attempts) {
  PoisKernelUnif kernel{};
  kernel.count.x = sample_count;

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> dist_radius(0.0f, 1.0f);
  std::uniform_real_distribution<float> dist_angle(0.0f,
                                                   2.0f * glm::pi<float>());

  float min_dist = 0.8f / std::sqrt(static_cast<float>(sample_count));
  kernel.samples[0] = glm::vec4(0.0f);

  for (uint32_t i = 1; i < sample_count; ++i) {
    bool found = false;

    for (uint32_t attempt = 0; attempt < max_attempts; ++attempt) {
      float r = std::sqrt(dist_radius(rng));
      float theta = dist_angle(rng);
      glm::vec2 candidate(r * std::cos(theta), r * std::sin(theta));

      bool valid = true;
      for (uint32_t j = 0; j < i; ++j) {
        if (glm::length(candidate - glm::vec2(kernel.samples[j])) < min_dist) {
          valid = false;
          break;
        }
      }

      if (valid) {
        kernel.samples[i] = glm::vec4(candidate, 0.0f, 0.0f);
        found = true;
        break;
      }
    }

    if (!found) {
      float r = std::sqrt(dist_radius(rng));
      float theta = dist_angle(rng);
      kernel.samples[i] =
          glm::vec4(r * std::cos(theta), r * std::sin(theta), 0.0f, 0.0f);
    }
  }

  return kernel;
}

// === create textures ===

void Resources::CreateDefaultColorTex(vk::CommandBuffer cmd_buff) {
  TextureProps tex_data;
  tex_data.name = "default_texture";
  tex_data.width = 1;
  tex_data.height = 1;
  tex_data.type = TextureType::kColor;

  default_color_tex_ = std::make_unique<Texture>(ctx_, tex_data);
  default_color_tex_->Write(cmd_buff, {255, 255, 255, 255});
}

vk::SampleCountFlagBits Resources::GetMaxSamples() {
  vk::PhysicalDeviceProperties props = ctx_.GetProperties();
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

void Resources::CreateWhiteNoiseTex(vk::CommandBuffer cmd_buff) {
  std::vector<uint8_t> noise(kWhiteNoiseDim * kWhiteNoiseDim);

  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<uint16_t> dist(0, 255);

  for (auto& n : noise) n = dist(rng);

  SamplerProps props;
  props.address_mode_U = vk::SamplerAddressMode::eRepeat;
  props.address_mode_V = vk::SamplerAddressMode::eRepeat;
  props.min_filter = props.mag_filter = vk::Filter::eNearest;

  white_noise_tex_ = std::make_unique<Texture>(
      ctx_, frame_props_.white_noise_format,
      vk::Extent2D{kWhiteNoiseDim, kWhiteNoiseDim},
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
      "white_noise_tex", props);
  white_noise_tex_->Write(cmd_buff, noise.data(),
                          sizeof(uint8_t) * noise.size());
}

void Resources::CreateAONoiseTex(vk::CommandBuffer cmd_buff) {
  std::vector<glm::vec2> noise(kAONoiseDim * kAONoiseDim);

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  for (auto& n : noise) n = glm::normalize(glm::vec2(dist(rng), dist(rng)));

  SamplerProps props;
  props.address_mode_U = vk::SamplerAddressMode::eRepeat;
  props.address_mode_V = vk::SamplerAddressMode::eRepeat;

  ao_noise_tex_ = std::make_unique<Texture>(
      ctx_, frame_props_.ao_noise_format,
      vk::Extent2D{kAONoiseDim, kAONoiseDim},
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1,
      "ao_noise_tex", props);

  ao_noise_tex_->Write(cmd_buff, noise.data(),
                       sizeof(glm::vec2) * noise.size());
}

void Resources::LoadBlueTextures(vk::CommandBuffer cmd_buff) {
  LoadTex(cmd_buff, "../assets/textures/blue_noise_64.png", blue_noise_tex_64_,
          "blue_noise_64");
  LoadTex(cmd_buff, "../assets/textures/blue_noise_128.png",
          blue_noise_tex_128_, "blue_noise_128");
  LoadTex(cmd_buff, "../assets/textures/blue_noise_256.png",
          blue_noise_tex_256_, "blue_noise_256");

  LoadTex(cmd_buff, "../assets/textures/blue_noise_64_1.png",
          blue_noise_tex_64_1, "blue_noise_64_1");
  LoadTex(cmd_buff, "../assets/textures/blue_noise_64_2.png",
          blue_noise_tex_64_2, "blue_noise_64_2");
  LoadTex(cmd_buff, "../assets/textures/blue_noise_64_3.png",
          blue_noise_tex_64_3, "blue_noise_64_3");
}

void Resources::LoadTex(vk::CommandBuffer cmd_buff, const std::string& filepath,
                        std::unique_ptr<Texture>& out_texture,
                        const std::string& name) {
  int width, height, channels;
  unsigned char* data =
      stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (!data)
    throw std::runtime_error("failed to load texture: " +
                             npr_core::GetFilename(filepath));

  SamplerProps props;
  props.address_mode_U = vk::SamplerAddressMode::eRepeat;
  props.address_mode_V = vk::SamplerAddressMode::eRepeat;
  props.min_filter = props.mag_filter = vk::Filter::eNearest;

  out_texture = std::make_unique<Texture>(
      ctx_, frame_props_.blue_noise_format, vk::Extent2D(width, height),
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      vk::ImageAspectFlagBits::eColor, vk::SampleCountFlagBits::e1, name,
      props);
  out_texture->Write(cmd_buff, data, width * height * 4);

  stbi_image_free(data);
}

// === create meshes ===

void Resources::CreateSphereMesh(vk::CommandBuffer cmd_buff) {
  const int segments = 16;
  const int rings = 16;

  std::vector<LightVertex> vertices;
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

      vertices.push_back({pos});
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

  sphere_mesh_ = std::make_unique<LightMesh>(
      VertexBuffer{ctx_, cmd_buff, vertices, "sphere_mesh_vbo"},
      IndexBuffer{ctx_, cmd_buff, indices, "sphere_mesh_ibo"});
}

void Resources::CreateConeMesh(vk::CommandBuffer cmd_buff) {
  const int segments = 32;

  std::vector<LightVertex> vertices;
  std::vector<uint32_t> indices;

  // tip vertex
  glm::vec3 tip_pos(0, 0, 0);
  vertices.emplace_back(tip_pos);
  int tip_idx = 0;

  glm::vec3 base_center_pos(0, 0, -1);
  vertices.emplace_back(base_center_pos);
  int base_center_idx = 1;

  // base ring vertices
  for (int i = 0; i < segments; ++i) {
    float angle = (float(i) / segments) * glm::two_pi<float>();
    float x = std::cos(angle);
    float y = std::sin(angle);

    glm::vec3 pos(x, y, -1);
    vertices.emplace_back(pos);
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

  cone_mesh_ = std::make_unique<LightMesh>(
      VertexBuffer{ctx_, cmd_buff, vertices, "cone_mesh_vbo"},
      IndexBuffer{ctx_, cmd_buff, indices, "cone_mesh_ibo"});
}

}  // namespace npr_graphics
