// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include "imgui.h"

#include "Common/MsgHandler.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "VideoCommon/ImGuiManager.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
static constexpr float SETTINGS_WINDOW_WIDTH = 640.0f;
static constexpr float SETTINGS_WINDOW_HEIGHT = 400.0f;
static constexpr float SETTINGS_WINDOW_MIDPOINT = 300.0f;

template <typename T>
static bool IsSettingOverridden(const Config::ConfigInfo<T>& setting)
{
  return Config::GetActiveLayerForConfig(setting) != Config::LayerType::Base;
}

template <typename T>
static void PushTextStyle(const Config::ConfigInfo<T>& setting)
{
  // Make gameini/netplay-overridden variables red.
  if (IsSettingOverridden(setting))
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.51f, 0.53f, 1.0f));
}

template <typename T>
static void PopTextStyle(const Config::ConfigInfo<T>& setting)
{
  if (IsSettingOverridden(setting))
    ImGui::PopStyleColor();
}

static void ReloadSettings()
{
  g_Config.LoadSettings();
  g_Config.VerifyValidity();
}

static void DrawGroupHeader(const std::string& label, bool first = false)
{
  if (!first)
    ImGui::NewLine();

  ImGui::Text("%s", label.c_str());
  ImGui::Separator();
}

template <typename T>
static void DrawLabel(const std::string& label, const Config::ConfigInfo<T>& setting)
{
  PushTextStyle(setting);
  ImGui::Text("%s", label.c_str());
  PopTextStyle(setting);

  ImGui::SameLine(SETTINGS_WINDOW_MIDPOINT);
}

static void DrawBooleanSetting(const std::string label, const Config::ConfigInfo<bool>& setting,
                               bool same_line, bool reversed = false)
{
  if (same_line)
    ImGui::SameLine(SETTINGS_WINDOW_MIDPOINT);

  bool value = Config::Get(setting) ^ reversed;
  PushTextStyle(setting);
  if (ImGui::Checkbox(label.c_str(), &value))
  {
    Config::SetBaseOrCurrent(setting, value ^ reversed);
    ReloadSettings();
  }
  PopTextStyle(setting);
}

template <typename T>
static void DrawComboSetting(const std::string label, const Config::ConfigInfo<T>& setting,
                             const char* options, float item_width = SETTINGS_WINDOW_MIDPOINT)
{
  DrawLabel(label, setting);

  ImGui::PushID(&setting);

  int value = static_cast<int>(Config::Get(setting));
  ImGui::SetNextItemWidth(item_width);
  if (ImGui::Combo("##", &value, options))
  {
    Config::SetBaseOrCurrent(setting, static_cast<T>(value));
    ReloadSettings();
  }

  ImGui::PopID();
}

template <typename T>
static void DrawRangeSetting(const std::string label, const Config::ConfigInfo<T>& setting, int min,
                             int max, float item_width = SETTINGS_WINDOW_MIDPOINT)
{
  DrawLabel(label, setting);

  ImGui::PushID(&setting);

  int value = static_cast<int>(Config::Get(setting));
  ImGui::SetNextItemWidth(item_width);
  if (ImGui::SliderInt("##", &value, min, max))
  {
    Config::SetBaseOrCurrent(setting, static_cast<T>(value));
    ReloadSettings();
  }

  ImGui::PopID();
}

static void DrawGeneralSettings()
{
  DrawGroupHeader(GetStringT("Basic"), true);

  DrawComboSetting(GetStringT("Aspect Ratio:"), Config::GFX_ASPECT_RATIO,
                   "Auto\0Force 16:9\0Force 4:3\0Stretch to Window\0");
  DrawBooleanSetting(GetStringT("V-Sync"), Config::GFX_VSYNC, false);

  DrawGroupHeader(GetStringT("Other"));
  DrawBooleanSetting(GetStringT("Show FPS"), Config::GFX_SHOW_FPS, false);
  DrawBooleanSetting(GetStringT("Show NetPlay Ping"), Config::GFX_SHOW_NETPLAY_PING, true);
  DrawBooleanSetting(GetStringT("Log Render Time to File"), Config::GFX_LOG_RENDER_TIME_TO_FILE,
                     false);
  DrawBooleanSetting(GetStringT("Show NetPlay Messages"), Config::GFX_SHOW_NETPLAY_MESSAGES, true);

  DrawGroupHeader(GetStringT("Shader Compilation"));
  {
    int mode = static_cast<int>(Config::Get(Config::GFX_SHADER_COMPILATION_MODE));
    bool changed = ImGui::RadioButton(GetStringT("Synchronous").c_str(), &mode, 0);
    ImGui::SameLine(SETTINGS_WINDOW_MIDPOINT);
    changed |= ImGui::RadioButton(GetStringT("Synchronous (Ubershaders)").c_str(), &mode, 1);
    changed |= ImGui::RadioButton(GetStringT("Asynchronous (Ubershaders)").c_str(), &mode, 2);
    ImGui::SameLine(SETTINGS_WINDOW_MIDPOINT);
    changed |= ImGui::RadioButton(GetStringT("Asynchronous (Skip Drawing)").c_str(), &mode, 3);
    if (changed)
    {
      Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE,
                               static_cast<ShaderCompilationMode>(mode));
      ReloadSettings();
    }
  }

  DrawBooleanSetting(GetStringT("Compile Shaders Before Starting"),
                     Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, false);
}

static void DrawEnhancementSettings()
{
  DrawGroupHeader(GetStringT("Enhancements"), true);
  DrawComboSetting(GetStringT("Internal Resolution:"), Config::GFX_EFB_SCALE,
                   "Auto(Multiple of 640x528)\0"
                   "Native(640x528)\0"
                   "2x Native (1280x1056) for 720p\0"
                   "3x Native (1920x1584) for 1080p\0"
                   "4x Native (2560x2112) for 1440p\0"
                   "5x Native (3200x2640)\0"
                   "6x Native (3840x3168) for 4K\0"
                   "7x Native (4480x3696)\0"
                   "8x Native (5120x4224) for 5K");

  {
    u32 current_samples = static_cast<u32>(Config::Get(Config::GFX_MSAA));
    bool current_ssaa = Config::Get(Config::GFX_SSAA);
    bool enabled = current_samples > 1;
    std::string current_value =
        enabled ? StringFromFormat("%ux %s", current_samples, current_ssaa ? "SSAA" : "MSAA") :
                  "None";
    bool changed = false;

    DrawLabel(GetStringT("Anti-Aliasing:"), Config::GFX_MSAA);
    ImGui::SetNextItemWidth(SETTINGS_WINDOW_MIDPOINT);
    if (ImGui::BeginCombo("##graphics_settings_msaa", current_value.c_str()))
    {
      if (ImGui::Selectable("None", !enabled))
      {
        current_samples = 1;
        changed = true;
      }

      // MSAA
      for (u32 samples : g_Config.backend_info.AAModes)
      {
        if (samples == 1)
          continue;

        if (ImGui::Selectable(StringFromFormat("%ux MSAA", samples).c_str(),
                              current_samples == samples && !current_ssaa))
        {
          current_samples = samples;
          current_ssaa = false;
          changed = true;
        }
      }

      // SSAA
      if (g_Config.backend_info.bSupportsSSAA)
      {
        for (u32 samples : g_Config.backend_info.AAModes)
        {
          if (samples == 1)
            continue;

          if (ImGui::Selectable(StringFromFormat("%ux SSAA", samples).c_str(),
                                current_samples == samples && current_ssaa))
          {
            current_samples = samples;
            current_ssaa = true;
            changed = true;
          }
        }
      }

      if (changed)
      {
        Config::SetBaseOrCurrent(Config::GFX_MSAA, current_samples);
        Config::SetBaseOrCurrent(Config::GFX_SSAA, current_ssaa);
        ReloadSettings();
      }

      ImGui::EndCombo();
    }
  }

  {
    static constexpr std::array<const char*, 5> af_options = {{"1x", "2x", "4x", "8x", "16x"}};
    int af_value = std::min(Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY),
                            static_cast<int>(af_options.size() - 1));
    DrawLabel(GetStringT("Anisotropic Filtering:"), Config::GFX_ENHANCE_MAX_ANISOTROPY);
    ImGui::SetNextItemWidth(SETTINGS_WINDOW_MIDPOINT);
    if (ImGui::Combo("##graphics_settings_af", &af_value, af_options.data(),
                     static_cast<int>(af_options.size())))
    {
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, af_value);
      ReloadSettings();
    }
  }

  DrawBooleanSetting(GetStringT("Scaled EFB Copy"), Config::GFX_HACK_COPY_EFB_SCALED, false);
  DrawBooleanSetting(GetStringT("Per-Pixel Lighting"), Config::GFX_ENABLE_PIXEL_LIGHTING, true);
  DrawBooleanSetting(GetStringT("Force Texture Filtering"), Config::GFX_ENHANCE_FORCE_FILTERING,
                     false);
  DrawBooleanSetting(GetStringT("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK, true);
  DrawBooleanSetting(GetStringT("Disable Fog"), Config::GFX_DISABLE_FOG, false);
  DrawBooleanSetting(GetStringT("Force 24-Bit Color"), Config::GFX_ENHANCE_FORCE_TRUE_COLOR, true);
  DrawBooleanSetting(GetStringT("Disable Copy Filter"), Config::GFX_ENHANCE_DISABLE_COPY_FILTER,
                     false);
  DrawBooleanSetting(GetStringT("Arbitrary Mipmap Detection"),
                     Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, true);

  DrawGroupHeader(GetStringT("Stereoscopy"));

  DrawComboSetting(GetStringT("Stereoscopic 3D Mode:"), Config::GFX_STEREO_MODE,
                   "Off\0Side-by-Side\0Top-and-Bottom\0Anaglyph\0HDMI 3D\0NVIDIA 3D Vision");
  DrawRangeSetting(GetStringT("Depth:"), Config::GFX_STEREO_DEPTH, 0, 100);
  DrawRangeSetting(GetStringT("Convergence:"), Config::GFX_STEREO_CONVERGENCE, 0, 200);
  DrawBooleanSetting(GetStringT("Swap Eyes"), Config::GFX_STEREO_SWAP_EYES, false);
}

static void DrawHacksSettings()
{
  DrawGroupHeader(GetStringT("Hacks"), true);

  DrawBooleanSetting(GetStringT("Skip EFB Access from CPU"), Config::GFX_HACK_EFB_ACCESS_ENABLE,
                     false, true);
  DrawBooleanSetting(GetStringT("Ignore Format Changes"),
                     Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, true, true);
  DrawBooleanSetting(GetStringT("Store EFB Copies to Texture Only"),
                     Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, false);
  DrawBooleanSetting(GetStringT("Defer EFB Copies to RAM"), Config::GFX_HACK_DEFER_EFB_COPIES,
                     true);

  DrawGroupHeader(GetStringT("Texture Cache"), true);

  {
    static constexpr std::array<int, 3> groups = {{0, 512, 128}};
    int samples = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
    int group = 0;
    for (int i = 0; i < static_cast<int>(groups.size()); i++)
    {
      if (groups[i] == samples)
      {
        group = i;
        break;
      }
    }

    DrawLabel(GetStringT("Accuracy:"), Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
    ImGui::SetNextItemWidth(SETTINGS_WINDOW_MIDPOINT);
    if (ImGui::Combo("##texture_cache_accuracy", &group, "Safe\0Moderate\0Fast"))
    {
      Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, groups[group]);
      ReloadSettings();
    }
  }

  DrawBooleanSetting(GetStringT("GPU Texture Decoding"), Config::GFX_ENABLE_GPU_TEXTURE_DECODING,
                     false);

  DrawGroupHeader(GetStringT("External Frame Buffer (XFB)"));
  DrawBooleanSetting(GetStringT("Store XFB Copies to Texture Only"),
                     Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, false);
  DrawBooleanSetting(GetStringT("Immediately Present XFB"), Config::GFX_HACK_IMMEDIATE_XFB, false);

  DrawGroupHeader(GetStringT("Other"));
  DrawBooleanSetting(GetStringT("Fast Depth Calculation"), Config::GFX_FAST_DEPTH_CALC, false);
  DrawBooleanSetting(GetStringT("Disable Bounding Box"), Config::GFX_HACK_BBOX_ENABLE, true, true);
  DrawBooleanSetting(GetStringT("Vertex Rounding"), Config::GFX_HACK_VERTEX_ROUDING, false);
}

static void DrawAdvancedSettings()
{
  DrawGroupHeader(GetStringT("Debugging"), true);
  DrawBooleanSetting(GetStringT("Enable Wireframe"), Config::GFX_ENABLE_WIREFRAME, false);
  DrawBooleanSetting(GetStringT("Show Statistics"), Config::GFX_OVERLAY_STATS, true);
  DrawBooleanSetting(GetStringT("Texture Format Overlay"), Config::GFX_TEXFMT_OVERLAY_ENABLE,
                     false);

  DrawGroupHeader(GetStringT("Utility"));
  DrawBooleanSetting(GetStringT("Dump Textures"), Config::GFX_DUMP_TEXTURES, false);
  DrawBooleanSetting(GetStringT("Load Custom Textures"), Config::GFX_HIRES_TEXTURES, true);
  DrawBooleanSetting(GetStringT("Prefetch Custom Textures"), Config::GFX_CACHE_HIRES_TEXTURES,
                     false);
  DrawBooleanSetting(GetStringT("Internal Resolution Frame Dumps"),
                     Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS, true);
  DrawBooleanSetting(GetStringT("Dump EFB Target"), Config::GFX_DUMP_EFB_TARGET, false);
  DrawBooleanSetting(GetStringT("Disable EFB VRAM Copies"), Config::GFX_HACK_DISABLE_COPY_TO_VRAM,
                     true);
  DrawBooleanSetting(GetStringT("Free Look"), Config::GFX_FREE_LOOK, false);
  DrawBooleanSetting(GetStringT("Frame Dumps Use FFV1"), Config::GFX_USE_FFV1, true);

  DrawGroupHeader(GetStringT("Misc"));
  DrawBooleanSetting(GetStringT("Crop"), Config::GFX_CROP, false);
  DrawBooleanSetting(GetStringT("Enable Progressive Scan"), Config::SYSCONF_PROGRESSIVE_SCAN, true);
  DrawBooleanSetting(GetStringT("Borderless Fullscreen"), Config::GFX_BORDERLESS_FULLSCREEN, false);

  DrawGroupHeader(GetStringT("Experimental"));
  DrawBooleanSetting(GetStringT("Defer EFB Cache Invalidation"),
                     Config::GFX_HACK_EFB_DEFER_INVALIDATION, false);
}

void ImGuiManager::DrawGraphicsSettingsWindow()
{
  ImGui::SetNextWindowSize(ImVec2(SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT), ImGuiCond_Once);
  if (!ImGui::Begin(GetStringT("Graphics Settings").c_str(), &m_graphics_settings_window_open))
  {
    ImGui::End();
    return;
  }

  if (ImGui::BeginTabBar("graphics_settings_tab_bar", ImGuiTabBarFlags_NoTooltip))
  {
    if (ImGui::BeginTabItem(GetStringT("General").c_str()))
    {
      DrawGeneralSettings();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(GetStringT("Enhancements").c_str()))
    {
      DrawEnhancementSettings();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(GetStringT("Hacks").c_str()))
    {
      DrawHacksSettings();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(GetStringT("Advanced").c_str()))
    {
      DrawAdvancedSettings();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
}  // namespace VideoCommon
