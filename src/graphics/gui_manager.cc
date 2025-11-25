#include "gui_manager.h"

using namespace npr_scene;

namespace npr_graphics {
PFN_vkVoidFunction GuiManager::VulkanLoaderFn(const char* fn_name,
                                              void* user_data) {
  auto* handles = static_cast<VulkanHandles*>(user_data);
  PFN_vkVoidFunction fn = nullptr;

  if (handles->instance) {
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
        handles->instance, fn_name);
    if (fn) return fn;
  }
  if (handles->device) {
    fn = vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr(
        handles->device, fn_name);
    if (fn) return fn;
  }
  return vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr(
      VK_NULL_HANDLE, fn_name);
}

void GuiManager::CheckVkResult(VkResult result) {
  if (result != VK_SUCCESS)
    throw std::runtime_error("Vulkan Error: VkResult = " +
                             std::to_string(result));
}

GuiManager::GuiManager(const npr_window::Window& window, const Context& ctx,
                       const Swapchain& swapchain, const SwapPass& swap_pass,
                       const PipelineCache& pipeline_cache)
    : desc_pool_{ctx} {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  io.FontDefault = io.Fonts->AddFontFromFileTTF(
      "../assets/fonts/JetBrainsMono-Medium.ttf", 17.0f);

  ImGui::GetStyle().TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;

  VulkanHandles handles{ctx.GetInstance(), ctx.GetDevice()};
  ImGui_ImplVulkan_LoadFunctions(ctx.GetAPIVer(), GuiManager::VulkanLoaderFn,
                                 &handles);

  ImGui_ImplGlfw_InitForVulkan(window.GetNative(), true);

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = ctx.GetAPIVer();
  init_info.Instance = ctx.GetInstance();
  init_info.PhysicalDevice = ctx.GetPhysDevice();
  init_info.Device = ctx.GetDevice();
  init_info.QueueFamily = ctx.GetQFamilies().graphics_i.value();
  init_info.Queue = ctx.GetGraphicsQ();
  init_info.PipelineCache = pipeline_cache.GetCache();
  init_info.DescriptorPool = desc_pool_.GetPool();
  init_info.RenderPass = swap_pass.GetRenderPass();
  init_info.Subpass = 1;
  init_info.MinImageCount = swapchain.GetProps().min_image_count;
  init_info.ImageCount = swapchain.GetProps().image_count;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = CheckVkResult;

  ImGui_ImplVulkan_Init(&init_info);
}

GuiManager::~GuiManager() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void GuiManager::NewFrame(const npr_core::FrameTimer& timer, Camera& camera,
                          Scene& scene) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  FpsOverlay(timer.GetAvgFPS());
  if (!camera.IsOrbit()) return;

  SceneWindow(camera, scene);
  InspectorWindow(camera);
}

void GuiManager::FpsOverlay(float fps) {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin(
      "overlay", nullptr,
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  ImGui::Text("FPS: %.1f", fps);
  ImGui::End();

  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
}

void GuiManager::SceneWindow(Camera& camera, Scene& scene) {
  ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Once);
  ImGui::Begin("scene", nullptr, ImGuiWindowFlags_None);

  // cameras
  if (ImGui::CollapsingHeader("cameras")) {
    scene.GetCamQuery().each([this, &camera](flecs::entity ent,
                                             const CameraTag&,
                                             const TransformComp&) {
      std::string name = ent.name().c_str();
      if (ImGui::Selectable(name.c_str(), selected_ent_ == ent)) {
        selected_ent_ = ent;
        camera.SetEntity(ent);
      }
    });
  }

  // lights
  if (ImGui::CollapsingHeader("lights")) {
    ImGui::SeparatorText("directional");
    ImGui::Spacing();

    scene.GetDirLightQuery().each([this](flecs::entity ent, const DirLightTag&,
                                         const TransformComp&,
                                         const LightComp&) {
      std::string name = ent.name().c_str();
      if (ImGui::Selectable(name.c_str(), selected_ent_ == ent)) {
        selected_ent_ = ent;
      }
    });

    ImGui::Spacing();
    ImGui::SeparatorText("point");
    ImGui::Spacing();

    scene.GetPointLightQuery().each(
        [this, &camera](flecs::entity ent, const PointLightTag&,
                        const TransformComp&, const LightComp&,
                        const RangeComp&, const BoundingBoxComp&) {
          if (ImGui::Selectable(ent.name().c_str(), selected_ent_ == ent)) {
            selected_ent_ = ent;
            camera.SetTrackTarget(ent);
          }
        });

    ImGui::Spacing();
    ImGui::SeparatorText("spot");
    ImGui::Spacing();

    scene.GetSpotLightQuery().each(
        [this, &camera](flecs::entity ent, const SpotLightTag&,
                        const TransformComp&, const LightComp&,
                        const RangeComp&, const SpotComp&,
                        const BoundingBoxComp&) {
          std::string name = ent.name().c_str();
          if (ImGui::Selectable(ent.name().c_str(), selected_ent_ == ent)) {
            selected_ent_ = ent;
            camera.SetTrackTarget(ent);
          }
        });

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("scene graph")) {
    ImGui::Spacing();

    ImGui::InputTextWithHint("##search", "search", search_buff_,
                             sizeof(search_buff_));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    std::string search = search_buff_;

    std::unordered_set<uint64_t> visib_ents;
    CalcVisib(scene, search, visib_ents);

    scene.GetObjectQuery().each(  // root objects
        [&](flecs::entity ent, const ObjectTag&, const TransformComp&) {
          if (!search.empty() && !visib_ents.contains(ent.id())) return;

          DrawEntTree(ent, camera, !search.empty(), visib_ents);
        });
  }

  ImGui::End();
}

void GuiManager::InspectorWindow(npr_scene::Camera& camera) {
  if (!selected_ent_.is_valid()) return;

  ImVec2 win_size{350, 400};
  ImVec2 display_size = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(display_size.x - win_size.x - 10, 40),
                          ImGuiCond_Once);
  ImGui::SetNextWindowSize(win_size, ImGuiCond_Once);

  bool open{true};
  ImGui::Begin("inspector", &open);

  ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s",
                     selected_ent_.name().c_str());
  ImGui::Separator();
  ImGui::Spacing();

  DrawTransformComp(camera);
  DrawMeshComp();
  DrawMaterialComp();
  DrawBoundingBoxComp();

  DrawOrthographicComp(camera);
  DrawPerspectiveComp(camera);

  DrawLightComp();
  DrawRangeComp();
  DrawSpotComp();
  DrawBoundingBoxComp();

  ImGui::End();

  if (!open) selected_ent_ = flecs::entity::null();
}

void GuiManager::DrawTransformComp(npr_scene::Camera& camera) {
  if (!selected_ent_.has<TransformComp>()) return;
  auto& tf = selected_ent_.get_mut<TransformComp>();

  ImGui::SeparatorText("transform");
  bool changed = false;

  changed |= ImGui::DragFloat3("pos", &tf.pos.x, 0.1f);

  glm::vec3 euler = glm::degrees(glm::eulerAngles(tf.rot));
  euler.y = glm::clamp(euler.y, -89.0f, 89.0f);
  if (ImGui::DragFloat3("rot", &euler.x, 1.0f)) {
    tf.rot = glm::quat(glm::radians(euler));
    changed = true;
  }

  changed |= ImGui::DragFloat3("scale", &tf.scale.x, 0.01f, 0.001f, 100.0f);

  ImGui::Spacing();

  if (changed) {
    selected_ent_.modified<TransformComp>();
    tf.dirty = true;

    camera.SetEntity(
        selected_ent_);  // if camera is tracking this entity, update
  }
}

void GuiManager::DrawMeshComp() {
  if (!selected_ent_.has<MeshComp>()) return;
  auto& mesh = selected_ent_.get<MeshComp>();

  ImGui::SeparatorText("mesh");

  ImGui::Text("VBO Index: %d", mesh.vbo_idx);
  ImGui::Text("IBO Index: %d", mesh.ibo_idx);

  ImGui::Spacing();
}

void GuiManager::DrawMaterialComp() {
  if (!selected_ent_.has<MaterialComp>()) return;
  auto& mat = selected_ent_.get_mut<MaterialComp>();

  ImGui::SeparatorText("material");

  ImGui::Text("color: %d", mat.color_map_idx);
  ImGui::Text("normal: %d", mat.normal_map_idx);
  ImGui::Text("metallic/roughness: %d", mat.metallic_roughness_map_idx);
  ImGui::Text("emissive: %d", mat.emissive_map_idx);

  ImGui::Spacing();

  ImGui::ColorEdit4("color", &mat.color_factor.x);
  ImGui::ColorEdit3("emissive", &mat.emissive_factor.x);
  ImGui::SliderFloat("metallic", &mat.metallic_factor, 0.0f, 1.0f);
  ImGui::SliderFloat("roughness", &mat.roughness_factor, 0.0f, 1.0f);
  ImGui::SliderFloat("alpha cutoff", &mat.alpha_cutoff, 0.0f, 1.0f);

  ImGui::Spacing();

  ImGui::Checkbox("double sided", &mat.double_sided);
  ImGui::Checkbox("opaque", &mat.is_opaque);
  ImGui::Checkbox("mask", &mat.is_mask);

  ImGui::Spacing();
}

void GuiManager::DrawBoundingBoxComp() {
  if (!selected_ent_.has<BoundingBoxComp>()) return;
  auto& bb = selected_ent_.get<BoundingBoxComp>();

  ImGui::SeparatorText("bounding box");

  ImGui::Text("center: (%.2f, %.2f, %.2f)", bb.center.x, bb.center.y,
              bb.center.z);
  ImGui::Text("extent: (%.2f, %.2f, %.2f)", bb.extent.x, bb.extent.y,
              bb.extent.z);

  ImGui::Spacing();

  ImGui::Text("min: (%.2f, %.2f, %.2f)", bb.min_pos.x, bb.min_pos.y,
              bb.min_pos.z);
  ImGui::Text("max: (%.2f, %.2f, %.2f)", bb.max_pos.x, bb.max_pos.y,
              bb.max_pos.z);

  ImGui::Spacing();
}

void GuiManager::DrawPerspectiveComp(npr_scene::Camera& camera) {
  if (!selected_ent_.has<PerspectiveComp>()) return;
  auto& persp = selected_ent_.get_mut<PerspectiveComp>();

  ImGui::SeparatorText("perspective");
  bool changed{false};

  float fov_degrees = glm::degrees(persp.fov);
  if (ImGui::SliderFloat("fov", &fov_degrees, 10.0f, 160.0f, "%.1f°")) {
    persp.fov = glm::radians(fov_degrees);
    changed = true;
  }

  changed |= ImGui::DragFloat("near", &persp.near, 0.005f, 0.001f, persp.far);
  changed |= ImGui::DragFloat("far", &persp.far, 1.0f, persp.near, 10000.0f);

  if (persp.aspect > 0)
    ImGui::Text("aspect: %.3f", persp.aspect);
  else
    ImGui::Text("aspect: Auto");

  ImGui::Spacing();

  if (changed) camera.SetEntity(selected_ent_);
}

void GuiManager::DrawOrthographicComp(npr_scene::Camera& camera) {
  if (!selected_ent_.has<OrthographicComp>()) return;
  auto& ortho = selected_ent_.get_mut<OrthographicComp>();

  bool changed = false;

  ImGui::SeparatorText("orthographic");

  changed |= ImGui::DragFloat("x mag", &ortho.xmag, 0.1f, 0.1f, 100.0f);
  changed |= ImGui::DragFloat("y mag", &ortho.ymag, 0.1f, 0.1f, 100.0f);
  changed |= ImGui::DragFloat("near", &ortho.near, 0.1f, 0.001f, ortho.far);
  changed |= ImGui::DragFloat("far", &ortho.far, 1.0f, ortho.near, 1000.0f);

  ImGui::Spacing();

  if (changed) camera.SetEntity(selected_ent_);
}

void GuiManager::DrawLightComp() {
  if (!selected_ent_.has<LightComp>()) return;
  auto& light = selected_ent_.get_mut<LightComp>();

  ImGui::SeparatorText("light");

  ImGui::ColorEdit3("color", &light.color.x);
  ImGui::DragFloat("intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

  ImGui::Spacing();
}

void GuiManager::DrawRangeComp() {
  if (!selected_ent_.has<RangeComp>()) return;
  auto& range_comp = selected_ent_.get_mut<RangeComp>();

  ImGui::SeparatorText("range");

  bool changed =
      ImGui::DragFloat("range", &range_comp.range, 0.1f, 0.0f, 1000.0f);
  if (range_comp.range == 0.0f) {
    ImGui::SameLine();
    ImGui::TextDisabled("(infinite)");
  }

  ImGui::Spacing();

  if (changed) {
    selected_ent_.modified<TransformComp>();

    if (selected_ent_.has<PointLightTag>()) {
      auto& tf = selected_ent_.get_mut<TransformComp>();
      tf.scale = glm::vec3(range_comp.range);
      tf.dirty = true;

    } else if (selected_ent_.has<SpotLightTag>()) {
      auto& spot_comp = selected_ent_.get<SpotComp>();
      auto base_radius =
          range_comp.range * std::tan(spot_comp.outer_cone_angle);

      auto& tf = selected_ent_.get_mut<TransformComp>();
      tf.scale = glm::vec3(base_radius, base_radius, range_comp.range);
      tf.dirty = true;
    }
  }
}

void GuiManager::DrawSpotComp() {
  if (!selected_ent_.has<SpotComp>()) return;
  auto& spot_comp = selected_ent_.get_mut<SpotComp>();

  ImGui::SeparatorText("spot");

  float inner_degrees = glm::degrees(spot_comp.inner_cone_angle);
  float outer_degrees = glm::degrees(spot_comp.outer_cone_angle);

  if (ImGui::SliderFloat("inner", &inner_degrees, 0.0f, 90.0f, "%.1f°"))
    spot_comp.inner_cone_angle = glm::radians(inner_degrees);

  bool changed = false;
  if (ImGui::SliderFloat("outer", &outer_degrees, 0.0f, 90.0f, "%.1f°")) {
    spot_comp.outer_cone_angle = glm::radians(outer_degrees);
    changed = true;
  }

  ImGui::Spacing();

  if (changed) {
    selected_ent_.modified<TransformComp>();

    auto range_comp = selected_ent_.get<RangeComp>();
    auto base_radius = range_comp.range * std::tan(spot_comp.outer_cone_angle);
    auto& tf = selected_ent_.get_mut<TransformComp>();

    tf.scale = glm::vec3(base_radius, base_radius, range_comp.range);
    tf.dirty = true;
  }
}

void GuiManager::CalcVisib(npr_scene::Scene& scene, const std::string& search,
                           std::unordered_set<uint64_t>& visib_ents) {
  auto search_low = npr_core::ToLower(search);

  scene.GetRenderableQuery().each(
      [&](flecs::entity ent, const TransformComp&, const PrimitiveTag&,
          const MeshComp&, const MaterialComp&, const BoundingBoxComp&) {
        auto name_low = npr_core::ToLower(ent.name().c_str());
        if (name_low.find(search_low) == std::string::npos) return;

        while (ent.is_valid() && !visib_ents.contains(ent.id())) {
          visib_ents.insert(ent.id());
          ent = ent.parent();
        }
      });
}

void GuiManager::DrawEntTree(
    flecs::entity ent, npr_scene::Camera& camera, bool filter,
    const std::unordered_set<uint64_t>& visible_entities) {
  if (!visible_entities.contains(ent.id())) return;

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
  if (filter) flags |= ImGuiTreeNodeFlags_DefaultOpen;
  if (selected_ent_ == ent) flags |= ImGuiTreeNodeFlags_Selected;

  auto name = ent.name().c_str();
  if (ent.has<PrimitiveTag>()) {
    if (ImGui::Selectable(name, selected_ent_ == ent,
                          ImGuiSelectableFlags_SpanAllColumns)) {
      selected_ent_ = ent;
      camera.SetTrackTarget(ent.parent());
    }

    return;
  }

  bool is_open = ImGui::TreeNodeEx(name, flags);
  if (ImGui::IsItemClicked()) {
    selected_ent_ = ent;
    camera.SetTrackTarget(ent);
  }

  if (is_open) {
    ent.children([&, this](flecs::entity child) {
      DrawEntTree(child, camera, filter, visible_entities);
    });
    ImGui::TreePop();
  }
}

}  // namespace npr_graphics
