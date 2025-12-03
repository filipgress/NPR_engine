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

void GuiManager::ToggleFileBrowser(
    std::function<void(std::string)> on_file_selected) {
  show_browser_ = !show_browser_;

  if (show_browser_ && !path_.has_value()) {
    on_file_selected_ = on_file_selected;
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");  // win fallback
    path_ = home ? home : std::filesystem::current_path();
    RefreshDirList();
  }
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

void GuiManager::NewFrame(npr_graphics::RenderSettings& settings,
                          npr_core::FrameTimer& timer, Camera& camera,
                          Scene& scene) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  FpsOverlay(timer.GetAvgFPS());
  if (!scene.IsValid() || !camera.IsOrbit()) return;

  if (show_browser_) BrowserWindow();

  GlobalSettingsWindow(settings, timer, camera.GetAspect());
  SceneWindow(camera, scene);
  InspectorWindow(camera);

  // ImGui::ShowDemoWindow();
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

void GuiManager::GlobalSettingsWindow(npr_graphics::RenderSettings& settings,
                                      npr_core::FrameTimer& timer,
                                      float cam_aspect) {
  ImVec2 win_size{350, 350};
  ImVec2 display_size = ImGui::GetIO().DisplaySize;

  ImGui::SetNextWindowPos(ImVec2(display_size.x - win_size.x - 10, 40),
                          ImGuiCond_Once);
  ImGui::SetNextWindowSize(win_size, ImGuiCond_Once);

  ImGui::Begin("settings");

  if (ImGui::CollapsingHeader("frame rate")) {
    ImGui::Spacing();

    bool enable_limit = (timer.GetTargetFPS() > 0);
    if (ImGui::Checkbox("##fps_limit", &enable_limit))
      enable_limit ? timer.SetTargetFPS(60) : timer.SetTargetFPS(0);

    ImGui::SameLine();

    ImGui::BeginDisabled(!enable_limit);
    int target_fps = timer.GetTargetFPS();
    target_fps = target_fps == 0 ? 60 : target_fps;
    if (ImGui::SliderInt("##fps_target", &target_fps, 30, 120, "%d FPS"))
      timer.SetTargetFPS(static_cast<uint>(target_fps));
    ImGui::EndDisabled();

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("resolution")) {
    static int width = settings.target_size.width;
    static int height = settings.target_size.height;
    static bool lock_aspect = false;

    ImGui::Spacing();

    ImGui::Checkbox("lock aspect", &lock_aspect);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(150.0f);
    int values[2] = {width, height};
    if (ImGui::DragInt2("##res", values, 1.0f, 20, 3840, "%d")) {
      int new_width = values[0];
      int new_height = values[1];

      if (lock_aspect) {
        if (new_width != width) {
          height = static_cast<int>(static_cast<float>(new_width) / cam_aspect);
          width = new_width;
        } else if (new_height != height) {
          width = static_cast<int>(static_cast<float>(new_height) * cam_aspect);
          height = new_height;
        }
      } else {
        width = new_width;
        height = new_height;
      }
    }
    ImGui::Spacing();

    bool resolution_changed =
        (width != static_cast<int>(settings.target_size.width) ||
         height != static_cast<int>(settings.target_size.height));

    ImGui::BeginDisabled(!resolution_changed);
    if (ImGui::Button("apply", ImVec2(80, 0))) {
      settings.dirty_target_size = true;
      settings.target_size = vk::Extent2D{static_cast<uint32_t>(width),
                                          static_cast<uint32_t>(height)};
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("reset", ImVec2(80, 0))) {
      width = settings.target_size.width;
      height = settings.target_size.height;
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("transparency")) {
    static int selected_mode = static_cast<int>(settings.trans_mode);

    ImGui::Spacing();
    if (ImGui::Selectable("disabled", selected_mode == 0)) {
      selected_mode = 0;
      settings.trans_mode = static_cast<TransparencyMode>(selected_mode);
    }

    if (ImGui::Selectable("linked list (A-buffer)", selected_mode == 1)) {
      selected_mode = 1;
      settings.trans_mode = static_cast<TransparencyMode>(selected_mode);
    }

    if (ImGui::Selectable("weighted blended (WBoit)", selected_mode == 2)) {
      selected_mode = 2;
      settings.trans_mode = static_cast<TransparencyMode>(selected_mode);
    }

    if (settings.trans_mode == TransparencyMode::kABuff) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::SliderFloat("alpha cutoff", &settings.alpha_cutoff, 0.0f, 1.0f);

      int avg_nodes = static_cast<int>(settings.abuff_avg_nodes);
      if (ImGui::SliderInt("fragments", &avg_nodes, 1, 16))
        settings.abuff_avg_nodes = static_cast<uint32_t>(avg_nodes);
      if (ImGui::IsItemDeactivatedAfterEdit()) settings.dirty_abuff_size = true;
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip(
            "memory allocation per sample. triggers buffer resize");

      int max_sorted = static_cast<int>(settings.abuff_sorted_nodes);
      if (ImGui::SliderInt("sorted", &max_sorted, 1, 16))
        settings.abuff_sorted_nodes = static_cast<uint32_t>(max_sorted);

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("maximum number of fragments to sort per sample");
    }

    if (settings.trans_mode == TransparencyMode::kWBoit) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::SliderFloat("alpha cutoff", &settings.alpha_cutoff, 0.0f, 1.0f);
      ImGui::SliderFloat("alpha multiplier", &settings.wboit_alpha_multiplier,
                         1.0f, 20.0f);
      ImGui::SliderFloat("alpha power", &settings.wboit_alpha_power, 1.0f,
                         5.0f);
      ImGui::SliderFloat("depth factor", &settings.wboit_depth_factor, 0.0f,
                         2.0f);
      ImGui::SliderFloat("depth power", &settings.wboit_depth_power, 0.5f,
                         6.0f);
      ImGui::SliderFloat("weight min", &settings.wboit_weight_min, 1e-4f, 1e-1f,
                         "%.5f", ImGuiSliderFlags_Logarithmic);
      ImGui::SliderFloat("weight max", &settings.wboit_weight_max, 1e2f, 1e4f,
                         "%.0f", ImGuiSliderFlags_Logarithmic);
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("lighting")) {
    ImGui::Spacing();
    ImGui::SeparatorText("shading model");
    ImGui::Spacing();

    static int selected_shading = settings.is_pbr ? 1 : 0;

    if (ImGui::Selectable("Blinn-Phong", selected_shading == 0)) {
      selected_shading = 0;
      settings.is_pbr = false;
    }

    if (ImGui::Selectable("PBR (Physically Based Rendering)",
                          selected_shading == 1)) {
      selected_shading = 1;
      settings.is_pbr = true;
    }

    // Blinn-Phong specific settings
    if (!settings.is_pbr) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::DragFloat("diffuse intensity", &settings.diff_int, 0.01f, 0.0f,
                       2.0f);
      ImGui::DragFloat("specular intensity", &settings.spec_int, 0.01f, 0.0f,
                       2.0f);

      ImGui::Spacing();
      ImGui::SeparatorText("rim light");
      ImGui::Spacing();

      ImGui::ColorEdit3("rim color", &settings.rim_color.x);
      ImGui::DragFloat("rim intensity", &settings.rim_intensity, 0.005f, 0.0f,
                       1.0f);

      if (settings.rim_intensity < 0.05f) settings.inv_rim = false;

      ImGui::BeginDisabled(settings.rim_intensity < 0.05f);
      ImGui::DragFloat("power", &settings.rim_power, 0.1f, 0.1f, 20.0f);
      ImGui::Checkbox("invert", &settings.inv_rim);
      ImGui::EndDisabled();

      if (settings.rim_intensity < 0.05f) {
        ImGui::SameLine();
        ImGui::Text("(ineffective)");
      }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("ambient");
    ImGui::Spacing();

    ImGui::ColorEdit3("color", &settings.ambient_color.x);
    ImGui::DragFloat("intensity##ambient", &settings.ambient_intensity, 0.005f,
                     0.0f, 1.0f);

    if (settings.ambient_intensity < 0.05f) settings.enable_ssao = false;

    ImGui::Spacing();

    ImGui::BeginDisabled(settings.ambient_intensity < 0.05f);
    ImGui::Checkbox("ambient occlusion", &settings.enable_ssao);
    ImGui::EndDisabled();

    if (settings.ambient_intensity < 0.05f) {
      ImGui::SameLine();
      ImGui::Text("(ineffective)");
    }

    ImGui::BeginDisabled(!settings.enable_ssao);
    ImGui::DragFloat("radius", &settings.ssao_radius, 0.01f, 0.1f, 2.0f);
    ImGui::DragFloat("bias", &settings.ssao_bias, 0.001f, 0.001f, 0.1f);
    ImGui::EndDisabled();
  }

  if (ImGui::CollapsingHeader("bloom")) {
    ImGui::Spacing();

    ImGui::Checkbox("enable bloom", &settings.enable_bloom);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginDisabled(!settings.enable_bloom);

    ImGui::DragFloat("threshold##bloom", &settings.bloom_threshold, 0.01f, 0.0f,
                     5.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("brightness level above which colors are extracted");

    ImGui::DragFloat("soft threshold", &settings.bloom_soft_threshold, 0.01f,
                     0.0f, 1.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip(
          "smoothness of threshold transition (0=hard, 1=very soft)");

    ImGui::DragFloat("intensity##bloom", &settings.bloom_intensity, 0.01f, 0.0f,
                     3.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("overall bloom strength multiplier");

    ImGui::EndDisabled();

    if (!settings.enable_bloom) {
      ImGui::SameLine();
      ImGui::TextDisabled("(disabled)");
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("depth of field")) {
    ImGui::Spacing();

    ImGui::Checkbox("enable", &settings.enable_dof);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginDisabled(!settings.enable_dof);

    static int selected_debug = static_cast<int>(settings.dof_debug_mode);

    if (ImGui::Selectable("normal##debug", selected_debug == 0)) {
      selected_debug = 0;
      settings.dof_debug_mode = static_cast<uint>(selected_debug);
    }

    if (ImGui::Selectable("show coc##debug", selected_debug == 1)) {
      selected_debug = 1;
      settings.dof_debug_mode = static_cast<uint>(selected_debug);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("red = foreground, blue = background");

    if (ImGui::Selectable("split screen##debug", selected_debug == 2)) {
      selected_debug = 2;
      settings.dof_debug_mode = static_cast<uint>(selected_debug);
    }

    if (ImGui::Selectable("coc_weight##debug", selected_debug == 3)) {
      selected_debug = 3;
      settings.dof_debug_mode = static_cast<uint>(selected_debug);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::DragFloat("distance", &settings.dof_focus_distance, 0.1f, 0.1f,
                     100.0f);
    ImGui::DragFloat("range", &settings.dof_focus_range, 0.01f, 0.01f, 20.0f);
    ImGui::SliderFloat("blur radius", &settings.dof_blur_radius, 1.0f, 20.0f);

    ImGui::Spacing();
    ImGui::SeparatorText("intensity");
    ImGui::Spacing();

    ImGui::SliderFloat("fg##intensity", &settings.dof_near_int, 0.0f, 2.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("maximum blur intensity for objects closer than focus");

    ImGui::SliderFloat("bg##intesity", &settings.dof_far_int, 0.0f, 2.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip(
          "maximum blur intensity for objects farther than focus");

    ImGui::Spacing();
    ImGui::SeparatorText("falloff");
    ImGui::Spacing();

    ImGui::DragFloat("fg##falloff", &settings.dof_near_falloff, 0.05f, 0.1f,
                     50.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip(
          "smaller = steeper gradient, larger = gentler gradient");

    ImGui::DragFloat("bg##falloff", &settings.dof_far_falloff, 0.05f, 0.1f,
                     50.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip(
          "smaller = steeper gradient, larger = gentler gradient");

    ImGui::Spacing();
    ImGui::SeparatorText("bilateral filter");
    ImGui::Spacing();

    ImGui::SliderFloat("threshold##filter", &settings.dof_coc_threshold, 0.0f,
                       0.2f, "%.3f");

    ImGui::SliderFloat("falloff", &settings.dof_coc_falloff, 1.0f, 1000.0f,
                       "%.1f");

    ImGui::EndDisabled();

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("post processing")) {
    ImGui::Spacing();

    ImGui::Checkbox("enable##post_processing", &settings.enable_post_process);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginDisabled(!settings.enable_post_process);

    // Pixelization
    ImGui::SeparatorText("pixelization");
    ImGui::Spacing();

    int pixel_size = static_cast<int>(settings.pixel_size);
    if (ImGui::SliderInt("size", &pixel_size, 1, 16))
      settings.pixel_size = static_cast<uint32_t>(pixel_size);

    ImGui::Spacing();

    // Dithering
    ImGui::SeparatorText("dithering");
    ImGui::Spacing();

    static int selected_dither = static_cast<int>(settings.dither_mode);

    if (ImGui::Selectable("none##dither", selected_dither == 0)) {
      selected_dither = 0;
      settings.dither_mode = static_cast<DitherMode>(selected_dither);
    }

    if (ImGui::Selectable("white noise##dither", selected_dither == 1)) {
      selected_dither = 1;
      settings.dither_mode = static_cast<DitherMode>(selected_dither);
    }

    if (ImGui::Selectable("ordered (Bayer matrix)##dither",
                          selected_dither == 2)) {
      selected_dither = 2;
      settings.dither_mode = static_cast<DitherMode>(selected_dither);
    }

    if (ImGui::Selectable("blue noise##dither", selected_dither == 3)) {
      selected_dither = 3;
      settings.dither_mode = static_cast<DitherMode>(selected_dither);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static int selected_bayer = 0;  // 0=2x2, 1=4x4, 2=8x8
    if (settings.dither_mode != DitherMode::kNone) {
      ImGui::DragFloat("strength", &settings.dither_strength, 0.005f, 0.0f,
                       1.0f);
    }

    if (settings.dither_mode == DitherMode::kBlueNoise) {
      ImGui::Spacing();

      int blue_noise_idx = static_cast<int>(settings.blue_noise_idx);
      if (ImGui::SliderInt("texture##blue_noise", &blue_noise_idx, 0, 3)) {
        settings.blue_noise_idx = static_cast<uint32_t>(blue_noise_idx);
      }

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        const char* texture_names[] = {"64x64 #1", "64x64 #2", "64x64 #3",
                                       "128x128", "256x256"};
        ImGui::SetTooltip("%s", texture_names[blue_noise_idx]);
      }

      ImGui::Spacing();
    }

    ImGui::Spacing();

    // Initialize from settings
    if (settings.dither_mode == DitherMode::kOrdered) {
      if (settings.bayer_size == 2)
        selected_bayer = 0;
      else if (settings.bayer_size == 4)
        selected_bayer = 1;
      else if (settings.bayer_size == 8)
        selected_bayer = 2;

      if (ImGui::Selectable("2x2##bayer", selected_bayer == 0)) {
        selected_bayer = 0;
        settings.bayer_size = 2;
      }
      if (ImGui::Selectable("4x4##bayer", selected_bayer == 1)) {
        selected_bayer = 1;
        settings.bayer_size = 4;
      }
      if (ImGui::Selectable("8x8##bayer", selected_bayer == 2)) {
        selected_bayer = 2;
        settings.bayer_size = 8;
      }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("color quantization");
    ImGui::Spacing();

    static int selected_quant = static_cast<int>(settings.quant_mode);

    if (ImGui::Selectable("none##quant", selected_quant == 0)) {
      selected_quant = 0;
      settings.quant_mode = static_cast<QuantMode>(selected_quant);
    }

    if (ImGui::Selectable("grayscale##quant", selected_quant == 1)) {
      selected_quant = 1;
      settings.quant_mode = static_cast<QuantMode>(selected_quant);
    }

    if (ImGui::Selectable("rgb##quant", selected_quant == 2)) {
      selected_quant = 2;
      settings.quant_mode = static_cast<QuantMode>(selected_quant);
    }

    if (ImGui::Selectable("palette luma##quant", selected_quant == 3)) {
      selected_quant = 3;
      settings.quant_mode = static_cast<QuantMode>(selected_quant);
    }

    if (ImGui::Selectable("palette nearest##quant", selected_quant == 4)) {
      selected_quant = 4;
      settings.quant_mode = static_cast<QuantMode>(selected_quant);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    int color_levels = static_cast<int>(settings.color_levels);

    if (settings.quant_mode == QuantMode::kGrayscale ||
        settings.quant_mode == QuantMode::kRGB) {
      if (ImGui::SliderInt("color levels", &color_levels, 1, 256))
        settings.color_levels =
            static_cast<uint32_t>(glm::clamp(color_levels, 2, 256));

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        if (settings.quant_mode == QuantMode::kGrayscale)
          ImGui::SetTooltip("number of gray shades\n2 = black & white");
        else if (settings.quant_mode == QuantMode::kRGB)
          ImGui::SetTooltip(
              "levels per channel (total colors = levels^3)\n"
              "2 = 8 colors, 4 = 64 colors, 8 = 512 colors");
      }
    }

    ImGui::Spacing();

    if (settings.quant_mode == QuantMode::kPaletteLuma ||
        settings.quant_mode == QuantMode::kPaletteNearest) {
      ImGui::Text("palette:");
      ImGui::Indent();

      const char* palettes[] = {"grayscale", "gameboy",  "commodore 64",
                                "nes",       "pico-8",   "warm",
                                "cool",      "sunset",   "earth",
                                "midnight",  "pastel-32"};

      int palette_idx = static_cast<int>(settings.palette_idx);
      if (ImGui::Combo("##palette_select", &palette_idx, palettes, 11))
        settings.palette_idx = static_cast<uint32_t>(palette_idx);

      ImGui::Unindent();
      ImGui::Spacing();
    }

    ImGui::Spacing();

    ImGui::EndDisabled();
  }

  ImGui::End();
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

    static char search_buff_[128] = "";
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

  ImVec2 win_size{300, 400};
  ImVec2 display_size = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(display_size.x - win_size.x - 10,
                                 display_size.y - win_size.y - 10),
                          ImGuiCond_Once);
  ImGui::SetNextWindowSize(win_size, ImGuiCond_Once);

  bool open{true};
  ImGui::Begin("inspector", &open);

  ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s",
                     selected_ent_.name().c_str());
  ImGui::Separator();
  ImGui::Spacing();

  DrawTransformComp(camera);
  DrawMaterialComp();

  DrawOrthographicComp(camera);
  DrawPerspectiveComp(camera);

  DrawLightComp();
  DrawRangeComp();
  DrawSpotComp();

  DrawMeshComp();
  DrawBoundingBoxComp();

  ImGui::End();

  if (!open) selected_ent_ = flecs::entity::null();
}

void GuiManager::BrowserWindow() {
  ImVec2 display_size = ImGui::GetIO().DisplaySize;
  ImVec2 window_size(400, 400);

  ImGui::SetNextWindowPos(ImVec2(display_size.x * 0.5f - window_size.x * 0.5f,
                                 display_size.y * 0.5f - window_size.y * 0.5f),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);

  ImGui::Begin("File Browser", &show_browser_,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoTitleBar);

  ImGui::TextWrapped("%s", path_.value().string().c_str());
  ImGui::Spacing();

  std::optional<std::filesystem::path> path_change;
  std::optional<std::filesystem::path> file_open;

  ImGui::BeginChild("FileList", ImVec2(0, 0), true);
  if (path_.value().has_parent_path() &&
      path_.value() != path_.value().root_path()) {
    if (ImGui::Selectable("[..]", false,
                          ImGuiSelectableFlags_DontClosePopups |
                              ImGuiSelectableFlags_AllowDoubleClick)) {
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        path_change = path_.value().parent_path();
    }
    ImGui::Spacing();
  }

  for (const auto& entry : dir_list_) {
    std::string display_name = entry.filename().string();

    bool is_dir = std::filesystem::is_directory(entry);
    if (is_dir) display_name = "[dir] " + display_name;

    if (ImGui::Selectable(display_name.c_str(),
                          selected_file_ == entry.string(),
                          ImGuiSelectableFlags_DontClosePopups |
                              ImGuiSelectableFlags_AllowDoubleClick)) {
      selected_file_ = entry.string();

      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (is_dir) {
          path_change = entry;
          selected_file_.clear();
        } else {
          file_open = entry;
        }
      }
    }
  }

  ImGui::EndChild();
  ImGui::End();

  if (path_change.has_value()) {
    path_ = path_change.value();
    RefreshDirList();
  }

  if (file_open.has_value()) {
    on_file_selected_(file_open->string());
    show_browser_ = false;
    selected_file_.clear();
  }
}

void GuiManager::RefreshDirList() {
  dir_list_.clear();

  try {
    if (!std::filesystem::exists(path_.value()) ||
        !std::filesystem::is_directory(path_.value()))
      path_ = std::filesystem::current_path();

    for (const auto& entry : std::filesystem::directory_iterator(path_.value()))
      if (entry.is_directory() || IsGlfwFile(entry.path()))
        dir_list_.push_back(entry.path());

    std::sort(
        dir_list_.begin(), dir_list_.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
          bool a_is_dir = std::filesystem::is_directory(a);
          bool b_is_dir = std::filesystem::is_directory(b);

          if (a_is_dir != b_is_dir) return a_is_dir;  // files after dirs
          return a.filename().string() < b.filename().string();  // alphabetical
        });

  } catch (const std::filesystem::filesystem_error& e) {
    ERR("while reading directory: ", e.what());
  }
}

bool GuiManager::IsGlfwFile(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  auto lower_ext = npr_core::ToLower(ext);
  return lower_ext == ".gltf" || lower_ext == ".glb";
}

void GuiManager::DrawTransformComp(npr_scene::Camera& camera) {
  if (!selected_ent_.has<TransformComp>()) return;
  auto& tf = selected_ent_.get_mut<TransformComp>();

  ImGui::SeparatorText("transform");
  bool changed = false;

  changed |= ImGui::DragFloat3("pos", &tf.pos.x, 0.1f);

  glm::vec3 euler = glm::degrees(glm::eulerAngles(tf.rot));
  euler.y = glm::clamp(euler.y, -180.0f, 180.0f);  // Prevent gimbal lock
  if (ImGui::DragFloat3("rot", &euler.x, 0.5f)) {
    euler.y = glm::clamp(euler.y, -89.0f, 89.0f);
    tf.rot = glm::normalize(glm::quat(glm::radians(euler)));
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

  ImGui::Text("color map: %d", mat.color_map_idx);
  ImGui::Text("normal map: %d", mat.normal_map_idx);
  ImGui::Text("metallic/roughness map: %d", mat.metallic_roughness_map_idx);
  ImGui::Text("emissive map: %d", mat.emissive_map_idx);

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
  ImGui::DragFloat("intensity##light", &light.intensity, 0.1f, 0.0f, 100.0f);

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

  if (ImGui::SliderFloat("inner", &inner_degrees, 0.0f, 60.0f, "%.1f°"))
    spot_comp.inner_cone_angle = glm::radians(inner_degrees);

  bool changed = false;
  if (ImGui::SliderFloat("outer", &outer_degrees, 0.0f, 60.0f, "%.1f°")) {
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
