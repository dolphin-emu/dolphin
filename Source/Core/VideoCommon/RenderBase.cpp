// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The GPU draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#include "VideoCommon/RenderBase.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Image.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Profiler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/FreeLookConfig.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/FrameDump.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

std::unique_ptr<Renderer> g_renderer;

static float AspectToWidescreen(float aspect)
{
  return aspect * ((16.0f / 9.0f) / (4.0f / 3.0f));
}

static bool DumpFrameToPNG(const FrameDump::FrameData& frame, const std::string& file_name)
{
  return Common::ConvertRGBAToRGBAndSavePNG(file_name, frame.data, frame.width, frame.height,
                                            frame.stride,
                                            Config::Get(Config::GFX_PNG_COMPRESSION_LEVEL));
}

Renderer::Renderer(int backbuffer_width, int backbuffer_height, float backbuffer_scale,
                   AbstractTextureFormat backbuffer_format)
    : m_backbuffer_width(backbuffer_width), m_backbuffer_height(backbuffer_height),
      m_backbuffer_scale(backbuffer_scale),
      m_backbuffer_format(backbuffer_format), m_last_xfb_width{MAX_XFB_WIDTH}, m_last_xfb_height{
                                                                                   MAX_XFB_HEIGHT}
{
  UpdateActiveConfig();
  FreeLook::UpdateActiveConfig();
  UpdateDrawRectangle();
  CalculateTargetSize();

  m_is_game_widescreen = SConfig::GetInstance().bWii && Config::Get(Config::SYSCONF_WIDESCREEN);
  g_freelook_camera.SetControlType(FreeLook::GetActiveConfig().camera_config.control_type);
}

Renderer::~Renderer() = default;

bool Renderer::Initialize()
{
  if (!InitializeImGui())
    return false;

  m_post_processor = std::make_unique<VideoCommon::PostProcessing>();
  if (!m_post_processor->Initialize(m_backbuffer_format))
    return false;

  m_bounding_box = CreateBoundingBox();
  if (g_ActiveConfig.backend_info.bSupportsBBox && !m_bounding_box->Initialize())
  {
    PanicAlertFmt("Failed to initialize bounding box.");
    return false;
  }

  if (g_ActiveConfig.bGraphicMods)
  {
    // If a config change occurred in a previous session,
    // remember the old change count value.  By setting
    // our current change count to the old value, we
    // avoid loading the stale data when we
    // check for config changes.
    const u32 old_game_mod_changes = g_ActiveConfig.graphics_mod_config ?
                                         g_ActiveConfig.graphics_mod_config->GetChangeCount() :
                                         0;
    g_ActiveConfig.graphics_mod_config = GraphicsModGroupConfig(SConfig::GetInstance().GetGameID());
    g_ActiveConfig.graphics_mod_config->Load();
    g_ActiveConfig.graphics_mod_config->SetChangeCount(old_game_mod_changes);
    m_graphics_mod_manager.Load(*g_ActiveConfig.graphics_mod_config);
  }

  return true;
}

void Renderer::Shutdown()
{
  // Disable ControllerInterface's aspect ratio adjustments so mapping dialog behaves normally.
  g_controller_interface.SetAspectRatioAdjustment(1);

  // First stop any framedumping, which might need to dump the last xfb frame. This process
  // can require additional graphics sub-systems so it needs to be done first
  ShutdownFrameDumping();
  ShutdownImGui();
  m_post_processor.reset();
  m_bounding_box.reset();
}

void Renderer::BeginUtilityDrawing()
{
  g_vertex_manager->Flush();
}

void Renderer::EndUtilityDrawing()
{
  // Reset framebuffer/scissor/viewport. Pipeline will be reset at next draw.
  g_framebuffer_manager->BindEFBFramebuffer();
  BPFunctions::SetScissorAndViewport();
}

void Renderer::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  m_current_framebuffer = framebuffer;
}

void Renderer::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  m_current_framebuffer = framebuffer;
}

void Renderer::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer,
                                      const ClearColor& color_value, float depth_value)
{
  m_current_framebuffer = framebuffer;
}

bool Renderer::EFBHasAlphaChannel() const
{
  return m_prev_efb_format == PixelFormat::RGBA6_Z24;
}

void Renderer::ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                           bool zEnable, u32 color, u32 z)
{
  g_framebuffer_manager->ClearEFB(rc, colorEnable, alphaEnable, zEnable, color, z);
}

void Renderer::ReinterpretPixelData(EFBReinterpretType convtype)
{
  g_framebuffer_manager->ReinterpretPixelData(convtype);
}

bool Renderer::IsBBoxEnabled() const
{
  return m_bounding_box->IsEnabled();
}

void Renderer::BBoxEnable()
{
  m_bounding_box->Enable();
}

void Renderer::BBoxDisable()
{
  m_bounding_box->Disable();
}

u16 Renderer::BBoxRead(u32 index)
{
  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
    return m_bounding_box_fallback[index];

  return m_bounding_box->Get(index);
}

void Renderer::BBoxWrite(u32 index, u16 value)
{
  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
  {
    m_bounding_box_fallback[index] = value;
    return;
  }

  m_bounding_box->Set(index, value);
}

void Renderer::BBoxFlush()
{
  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  m_bounding_box->Flush();
}

u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
  if (type == EFBAccessType::PeekColor)
  {
    u32 color = g_framebuffer_manager->PeekEFBColor(x, y);

    // a little-endian value is expected to be returned
    color = ((color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000));

    if (bpmem.zcontrol.pixel_format == PixelFormat::RGBA6_Z24)
    {
      color = RGBA8ToRGBA6ToRGBA8(color);
    }
    else if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16)
    {
      color = RGBA8ToRGB565ToRGBA8(color);
    }
    if (bpmem.zcontrol.pixel_format != PixelFormat::RGBA6_Z24)
    {
      color |= 0xFF000000;
    }

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::AlphaReadMode alpha_read_mode = PixelEngine::GetAlphaReadMode();

    if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadNone)
    {
      return color;
    }
    else if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadFF)
    {
      return color | 0xFF000000;
    }
    else
    {
      if (alpha_read_mode != PixelEngine::AlphaReadMode::Read00)
      {
        PanicAlertFmt("Invalid PE alpha read mode: {}", static_cast<u16>(alpha_read_mode));
      }
      return color & 0x00FFFFFF;
    }
  }
  else  // if (type == EFBAccessType::PeekZ)
  {
    // Depth buffer is inverted for improved precision near far plane
    float depth = g_framebuffer_manager->PeekEFBDepth(x, y);
    if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      depth = 1.0f - depth;

    // Convert to 24bit depth
    u32 z24depth = std::clamp<u32>(static_cast<u32>(depth * 16777216.0f), 0, 0xFFFFFF);

    if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16)
    {
      // When in RGB565_Z16 mode, EFB Z peeks return a 16bit value, which is presumably a
      // resolved sample from the MSAA buffer.
      // Dolphin doesn't currently emulate the 3 sample MSAA mode (and potentially never will)
      // it just transparently upgrades the framebuffer to 24bit depth and color and whatever
      // level of MSAA and higher Internal Resolution the user has configured.

      // This is mostly transparent, unless the game does an EFB read.
      // But we can simply convert the 24bit depth on the fly to the 16bit depth the game expects.

      return CompressZ16(z24depth, bpmem.zcontrol.zformat);
    }

    return z24depth;
  }
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  if (type == EFBAccessType::PokeColor)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to expected format (BGRA->RGBA)
      // TODO: Check alpha, depending on mode?
      const EfbPokeData& point = points[i];
      u32 color = ((point.data & 0xFF00FF00) | ((point.data >> 16) & 0xFF) |
                   ((point.data << 16) & 0xFF0000));
      g_framebuffer_manager->PokeEFBColor(point.x, point.y, color);
    }
  }
  else  // if (type == EFBAccessType::PokeZ)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to floating-point depth.
      const EfbPokeData& point = points[i];
      float depth = float(point.data & 0xFFFFFF) / 16777216.0f;
      if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
        depth = 1.0f - depth;

      g_framebuffer_manager->PokeEFBDepth(point.x, point.y, depth);
    }
  }
}

void Renderer::RenderToXFB(u32 xfbAddr, const MathUtil::Rectangle<int>& sourceRc, u32 fbStride,
                           u32 fbHeight, float Gamma)
{
  CheckFifoRecording();

  if (!fbStride || !fbHeight)
    return;
}

unsigned int Renderer::GetEFBScale() const
{
  return m_efb_scale;
}

int Renderer::EFBToScaledX(int x) const
{
  return x * static_cast<int>(m_efb_scale);
}

int Renderer::EFBToScaledY(int y) const
{
  return y * static_cast<int>(m_efb_scale);
}

float Renderer::EFBToScaledXf(float x) const
{
  return x * ((float)GetTargetWidth() / (float)EFB_WIDTH);
}

float Renderer::EFBToScaledYf(float y) const
{
  return y * ((float)GetTargetHeight() / (float)EFB_HEIGHT);
}

std::tuple<int, int> Renderer::CalculateTargetScale(int x, int y) const
{
  return std::make_tuple(x * static_cast<int>(m_efb_scale), y * static_cast<int>(m_efb_scale));
}

// return true if target size changed
bool Renderer::CalculateTargetSize()
{
  if (g_ActiveConfig.iEFBScale == EFB_SCALE_AUTO_INTEGRAL)
  {
    // Set a scale based on the window size
    int width = EFB_WIDTH * m_target_rectangle.GetWidth() / m_last_xfb_width;
    int height = EFB_HEIGHT * m_target_rectangle.GetHeight() / m_last_xfb_height;
    m_efb_scale = std::max((width - 1) / EFB_WIDTH + 1, (height - 1) / EFB_HEIGHT + 1);
  }
  else
  {
    m_efb_scale = g_ActiveConfig.iEFBScale;
  }

  const u32 max_size = g_ActiveConfig.backend_info.MaxTextureSize;
  if (max_size < EFB_WIDTH * m_efb_scale)
    m_efb_scale = max_size / EFB_WIDTH;

  auto [new_efb_width, new_efb_height] = CalculateTargetScale(EFB_WIDTH, EFB_HEIGHT);
  new_efb_width = std::max(new_efb_width, 1);
  new_efb_height = std::max(new_efb_height, 1);

  if (new_efb_width != m_target_width || new_efb_height != m_target_height)
  {
    m_target_width = new_efb_width;
    m_target_height = new_efb_height;
    PixelShaderManager::SetEfbScaleChanged(EFBToScaledXf(1), EFBToScaledYf(1));
    return true;
  }
  return false;
}

std::tuple<MathUtil::Rectangle<int>, MathUtil::Rectangle<int>>
Renderer::ConvertStereoRectangle(const MathUtil::Rectangle<int>& rc) const
{
  // Resize target to half its original size
  auto draw_rc = rc;
  if (g_ActiveConfig.stereo_mode == StereoMode::TAB)
  {
    // The height may be negative due to flipped rectangles
    int height = rc.bottom - rc.top;
    draw_rc.top += height / 4;
    draw_rc.bottom -= height / 4;
  }
  else
  {
    int width = rc.right - rc.left;
    draw_rc.left += width / 4;
    draw_rc.right -= width / 4;
  }

  // Create two target rectangle offset to the sides of the backbuffer
  auto left_rc = draw_rc;
  auto right_rc = draw_rc;
  if (g_ActiveConfig.stereo_mode == StereoMode::TAB)
  {
    left_rc.top -= m_backbuffer_height / 4;
    left_rc.bottom -= m_backbuffer_height / 4;
    right_rc.top += m_backbuffer_height / 4;
    right_rc.bottom += m_backbuffer_height / 4;
  }
  else
  {
    left_rc.left -= m_backbuffer_width / 4;
    left_rc.right -= m_backbuffer_width / 4;
    right_rc.left += m_backbuffer_width / 4;
    right_rc.right += m_backbuffer_width / 4;
  }

  return std::make_tuple(left_rc, right_rc);
}

void Renderer::SaveScreenshot(std::string filename)
{
  std::lock_guard<std::mutex> lk(m_screenshot_lock);
  m_screenshot_name = std::move(filename);
  m_screenshot_request.Set();
}

void Renderer::CheckForConfigChanges()
{
  const ShaderHostConfig old_shader_host_config = ShaderHostConfig::GetCurrent();
  const StereoMode old_stereo = g_ActiveConfig.stereo_mode;
  const u32 old_multisamples = g_ActiveConfig.iMultisamples;
  const int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  const int old_efb_access_tile_size = g_ActiveConfig.iEFBAccessTileSize;
  const bool old_force_filtering = g_ActiveConfig.bForceFiltering;
  const bool old_vsync = g_ActiveConfig.bVSyncActive;
  const bool old_bbox = g_ActiveConfig.bBBoxEnable;
  const u32 old_game_mod_changes =
      g_ActiveConfig.graphics_mod_config ? g_ActiveConfig.graphics_mod_config->GetChangeCount() : 0;
  const bool old_graphics_mods_enabled = g_ActiveConfig.bGraphicMods;

  UpdateActiveConfig();
  FreeLook::UpdateActiveConfig();
  g_vertex_manager->OnConfigChange();

  g_freelook_camera.SetControlType(FreeLook::GetActiveConfig().camera_config.control_type);

  if (g_ActiveConfig.bGraphicMods && !old_graphics_mods_enabled)
  {
    g_ActiveConfig.graphics_mod_config = GraphicsModGroupConfig(SConfig::GetInstance().GetGameID());
    g_ActiveConfig.graphics_mod_config->Load();
  }

  if (g_ActiveConfig.graphics_mod_config &&
      (old_game_mod_changes != g_ActiveConfig.graphics_mod_config->GetChangeCount()))
  {
    m_graphics_mod_manager.Load(*g_ActiveConfig.graphics_mod_config);
  }

  // Update texture cache settings with any changed options.
  g_texture_cache->OnConfigChanged(g_ActiveConfig);

  // EFB tile cache doesn't need to notify the backend.
  if (old_efb_access_tile_size != g_ActiveConfig.iEFBAccessTileSize)
    g_framebuffer_manager->SetEFBCacheTileSize(std::max(g_ActiveConfig.iEFBAccessTileSize, 0));

  // Check for post-processing shader changes. Done up here as it doesn't affect anything outside
  // the post-processor. Note that options are applied every frame, so no need to check those.
  if (m_post_processor->GetConfig()->GetShader() != g_ActiveConfig.sPostProcessingShader)
  {
    // The existing shader must not be in use when it's destroyed
    WaitForGPUIdle();

    m_post_processor->RecompileShader();
  }

  // Determine which (if any) settings have changed.
  ShaderHostConfig new_host_config = ShaderHostConfig::GetCurrent();
  u32 changed_bits = 0;
  if (old_shader_host_config.bits != new_host_config.bits)
    changed_bits |= CONFIG_CHANGE_BIT_HOST_CONFIG;
  if (old_stereo != g_ActiveConfig.stereo_mode)
    changed_bits |= CONFIG_CHANGE_BIT_STEREO_MODE;
  if (old_multisamples != g_ActiveConfig.iMultisamples)
    changed_bits |= CONFIG_CHANGE_BIT_MULTISAMPLES;
  if (old_anisotropy != g_ActiveConfig.iMaxAnisotropy)
    changed_bits |= CONFIG_CHANGE_BIT_ANISOTROPY;
  if (old_force_filtering != g_ActiveConfig.bForceFiltering)
    changed_bits |= CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING;
  if (old_vsync != g_ActiveConfig.bVSyncActive)
    changed_bits |= CONFIG_CHANGE_BIT_VSYNC;
  if (old_bbox != g_ActiveConfig.bBBoxEnable)
    changed_bits |= CONFIG_CHANGE_BIT_BBOX;
  if (CalculateTargetSize())
    changed_bits |= CONFIG_CHANGE_BIT_TARGET_SIZE;

  // No changes?
  if (changed_bits == 0)
    return;

  // Notify the backend of the changes, if any.
  OnConfigChanged(changed_bits);

  // If there's any shader changes, wait for the GPU to finish before destroying anything.
  if (changed_bits & (CONFIG_CHANGE_BIT_HOST_CONFIG | CONFIG_CHANGE_BIT_MULTISAMPLES))
  {
    WaitForGPUIdle();
    SetPipeline(nullptr);
  }

  // Framebuffer changed?
  if (changed_bits & (CONFIG_CHANGE_BIT_MULTISAMPLES | CONFIG_CHANGE_BIT_STEREO_MODE |
                      CONFIG_CHANGE_BIT_TARGET_SIZE))
  {
    g_framebuffer_manager->RecreateEFBFramebuffer();
  }

  // Reload shaders if host config has changed.
  if (changed_bits & (CONFIG_CHANGE_BIT_HOST_CONFIG | CONFIG_CHANGE_BIT_MULTISAMPLES))
  {
    OSD::AddMessage("Video config changed, reloading shaders.", OSD::Duration::NORMAL);
    g_vertex_manager->InvalidatePipelineObject();
    g_shader_cache->SetHostConfig(new_host_config);
    g_shader_cache->Reload();
    g_framebuffer_manager->RecompileShaders();
  }

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  if (changed_bits & CONFIG_CHANGE_BIT_TARGET_SIZE)
  {
    BPFunctions::SetScissorAndViewport();
  }

  // Stereo mode change requires recompiling our post processing pipeline and imgui pipelines for
  // rendering the UI.
  if (changed_bits & CONFIG_CHANGE_BIT_STEREO_MODE)
  {
    RecompileImGuiPipeline();
    m_post_processor->RecompilePipeline();
  }
}

// Create On-Screen-Messages
void Renderer::DrawDebugText()
{
  if (g_ActiveConfig.bShowFPS || g_ActiveConfig.bShowVPS || g_ActiveConfig.bShowSpeed)
  {
    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - (10.0f * m_backbuffer_scale),
                                   10.f * m_backbuffer_scale),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    int count = g_ActiveConfig.bShowFPS + g_ActiveConfig.bShowVPS + g_ActiveConfig.bShowSpeed;
    ImGui::SetNextWindowSize(
        ImVec2(94.f * m_backbuffer_scale, (12.f + 17.f * count) * m_backbuffer_scale));

    if (ImGui::Begin("Performance", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      const double fps = m_fps_counter.GetFPS();
      const double vps = m_vps_counter.GetFPS();
      const double speed = 100.0 * vps / VideoInterface::GetTargetRefreshRate();

      // Change Color based on % Speed
      float r = 0.0f, g = 1.0f, b = 1.0f;
      if (g_ActiveConfig.bShowSpeedColors)
      {
        r = 1.0 - (speed - 80.0) / 20.0;
        g = speed / 80.0;
        b = (speed - 90.0) / 10.0;
      }

      if (g_ActiveConfig.bShowFPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "FPS:%7.2lf", fps);
      if (g_ActiveConfig.bShowVPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "VPS:%7.2lf", vps);
      if (g_ActiveConfig.bShowSpeed)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Speed:%4.0lf%%", speed);
    }
    ImGui::End();
  }

  const bool show_movie_window =
      Config::Get(Config::MAIN_SHOW_FRAME_COUNT) || Config::Get(Config::MAIN_SHOW_LAG) ||
      Config::Get(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY) ||
      Config::Get(Config::MAIN_MOVIE_SHOW_RTC) || Config::Get(Config::MAIN_MOVIE_SHOW_RERECORD);
  if (show_movie_window)
  {
    // Position under the FPS display.
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - 10.f * m_backbuffer_scale, 80.f * m_backbuffer_scale),
        ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(150.0f * m_backbuffer_scale, 20.0f * m_backbuffer_scale),
        ImGui::GetIO().DisplaySize);
    if (ImGui::Begin("Movie", nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
      if (Movie::IsPlayingInput())
      {
        ImGui::Text("Frame: %" PRIu64 " / %" PRIu64, Movie::GetCurrentFrame(),
                    Movie::GetTotalFrames());
        ImGui::Text("Input: %" PRIu64 " / %" PRIu64, Movie::GetCurrentInputCount(),
                    Movie::GetTotalInputCount());
      }
      else if (Config::Get(Config::MAIN_SHOW_FRAME_COUNT))
      {
        ImGui::Text("Frame: %" PRIu64, Movie::GetCurrentFrame());
        ImGui::Text("Input: %" PRIu64, Movie::GetCurrentInputCount());
      }
      if (Config::Get(Config::MAIN_SHOW_LAG))
        ImGui::Text("Lag: %" PRIu64 "\n", Movie::GetCurrentLagCount());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY))
        ImGui::TextUnformatted(Movie::GetInputDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RTC))
        ImGui::TextUnformatted(Movie::GetRTCDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RERECORD))
        ImGui::TextUnformatted(Movie::GetRerecords().c_str());
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bOverlayStats)
    g_stats.Display();

  if (g_ActiveConfig.bShowNetPlayMessages && g_netplay_chat_ui)
    g_netplay_chat_ui->Display();

  if (Config::Get(Config::NETPLAY_GOLF_MODE_OVERLAY) && g_netplay_golf_ui)
    g_netplay_golf_ui->Display();

  if (g_ActiveConfig.bOverlayProjStats)
    g_stats.DisplayProj();

  if (g_ActiveConfig.bOverlayScissorStats)
    g_stats.DisplayScissor();

  const std::string profile_output = Common::Profiler::ToString();
  if (!profile_output.empty())
    ImGui::TextUnformatted(profile_output.c_str());
}

float Renderer::CalculateDrawAspectRatio() const
{
  const auto aspect_mode = g_ActiveConfig.aspect_mode;

  // If stretch is enabled, we prefer the aspect ratio of the window.
  if (aspect_mode == AspectMode::Stretch)
    return (static_cast<float>(m_backbuffer_width) / static_cast<float>(m_backbuffer_height));

  const float aspect_ratio = VideoInterface::GetAspectRatio();

  if (aspect_mode == AspectMode::AnalogWide ||
      (aspect_mode == AspectMode::Auto && m_is_game_widescreen))
  {
    return AspectToWidescreen(aspect_ratio);
  }

  return aspect_ratio;
}

void Renderer::AdjustRectanglesToFitBounds(MathUtil::Rectangle<int>* target_rect,
                                           MathUtil::Rectangle<int>* source_rect, int fb_width,
                                           int fb_height)
{
  const int orig_target_width = target_rect->GetWidth();
  const int orig_target_height = target_rect->GetHeight();
  const int orig_source_width = source_rect->GetWidth();
  const int orig_source_height = source_rect->GetHeight();
  if (target_rect->left < 0)
  {
    const int offset = -target_rect->left;
    target_rect->left = 0;
    source_rect->left += offset * orig_source_width / orig_target_width;
  }
  if (target_rect->right > fb_width)
  {
    const int offset = target_rect->right - fb_width;
    target_rect->right -= offset;
    source_rect->right -= offset * orig_source_width / orig_target_width;
  }
  if (target_rect->top < 0)
  {
    const int offset = -target_rect->top;
    target_rect->top = 0;
    source_rect->top += offset * orig_source_height / orig_target_height;
  }
  if (target_rect->bottom > fb_height)
  {
    const int offset = target_rect->bottom - fb_height;
    target_rect->bottom -= offset;
    source_rect->bottom -= offset * orig_source_height / orig_target_height;
  }
}

bool Renderer::IsHeadless() const
{
  return true;
}

void Renderer::ChangeSurface(void* new_surface_handle)
{
  std::lock_guard<std::mutex> lock(m_swap_mutex);
  m_new_surface_handle = new_surface_handle;
  m_surface_changed.Set();
}

void Renderer::ResizeSurface()
{
  std::lock_guard<std::mutex> lock(m_swap_mutex);
  m_surface_resized.Set();
}

void Renderer::SetViewportAndScissor(const MathUtil::Rectangle<int>& rect, float min_depth,
                                     float max_depth)
{
  SetViewport(static_cast<float>(rect.left), static_cast<float>(rect.top),
              static_cast<float>(rect.GetWidth()), static_cast<float>(rect.GetHeight()), min_depth,
              max_depth);
  SetScissorRect(rect);
}

void Renderer::ScaleTexture(AbstractFramebuffer* dst_framebuffer,
                            const MathUtil::Rectangle<int>& dst_rect,
                            const AbstractTexture* src_texture,
                            const MathUtil::Rectangle<int>& src_rect)
{
  ASSERT(dst_framebuffer->GetColorFormat() == AbstractTextureFormat::RGBA8);

  BeginUtilityDrawing();

  // The shader needs to know the source rectangle.
  const auto converted_src_rect =
      ConvertFramebufferRectangle(src_rect, src_texture->GetWidth(), src_texture->GetHeight());
  const float rcp_src_width = 1.0f / src_texture->GetWidth();
  const float rcp_src_height = 1.0f / src_texture->GetHeight();
  const std::array<float, 4> uniforms = {{converted_src_rect.left * rcp_src_width,
                                          converted_src_rect.top * rcp_src_height,
                                          converted_src_rect.GetWidth() * rcp_src_width,
                                          converted_src_rect.GetHeight() * rcp_src_height}};
  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

  // Discard if we're overwriting the whole thing.
  if (static_cast<u32>(dst_rect.GetWidth()) == dst_framebuffer->GetWidth() &&
      static_cast<u32>(dst_rect.GetHeight()) == dst_framebuffer->GetHeight())
  {
    SetAndDiscardFramebuffer(dst_framebuffer);
  }
  else
  {
    SetFramebuffer(dst_framebuffer);
  }

  SetViewportAndScissor(ConvertFramebufferRectangle(dst_rect, dst_framebuffer));
  SetPipeline(dst_framebuffer->GetLayers() > 1 ? g_shader_cache->GetRGBA8StereoCopyPipeline() :
                                                 g_shader_cache->GetRGBA8CopyPipeline());
  SetTexture(0, src_texture);
  SetSamplerState(0, RenderState::GetLinearSamplerState());
  Draw(0, 3);
  EndUtilityDrawing();
  if (dst_framebuffer->GetColorAttachment())
    dst_framebuffer->GetColorAttachment()->FinishedRendering();
}

MathUtil::Rectangle<int>
Renderer::ConvertFramebufferRectangle(const MathUtil::Rectangle<int>& rect,
                                      const AbstractFramebuffer* framebuffer) const
{
  return ConvertFramebufferRectangle(rect, framebuffer->GetWidth(), framebuffer->GetHeight());
}

MathUtil::Rectangle<int> Renderer::ConvertFramebufferRectangle(const MathUtil::Rectangle<int>& rect,
                                                               u32 fb_width, u32 fb_height) const
{
  MathUtil::Rectangle<int> ret = rect;
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
  {
    ret.top = fb_height - rect.bottom;
    ret.bottom = fb_height - rect.top;
  }
  return ret;
}

MathUtil::Rectangle<int> Renderer::ConvertEFBRectangle(const MathUtil::Rectangle<int>& rc) const
{
  MathUtil::Rectangle<int> result;
  result.left = EFBToScaledX(rc.left);
  result.top = EFBToScaledY(rc.top);
  result.right = EFBToScaledX(rc.right);
  result.bottom = EFBToScaledY(rc.bottom);
  return result;
}

std::tuple<float, float> Renderer::ScaleToDisplayAspectRatio(const int width,
                                                             const int height) const
{
  // Scale either the width or height depending the content aspect ratio.
  // This way we preserve as much resolution as possible when scaling.
  float scaled_width = static_cast<float>(width);
  float scaled_height = static_cast<float>(height);
  const float draw_aspect = CalculateDrawAspectRatio();
  if (scaled_width / scaled_height >= draw_aspect)
    scaled_height = scaled_width / draw_aspect;
  else
    scaled_width = scaled_height * draw_aspect;
  return std::make_tuple(scaled_width, scaled_height);
}

void Renderer::UpdateDrawRectangle()
{
  const float draw_aspect_ratio = CalculateDrawAspectRatio();

  // Update aspect ratio hack values
  // Won't take effect until next frame
  // Don't know if there is a better place for this code so there isn't a 1 frame delay
  if (g_ActiveConfig.bWidescreenHack)
  {
    float source_aspect = VideoInterface::GetAspectRatio();
    if (m_is_game_widescreen)
      source_aspect = AspectToWidescreen(source_aspect);

    const float adjust = source_aspect / draw_aspect_ratio;
    if (adjust > 1)
    {
      // Vert+
      g_Config.fAspectRatioHackW = 1;
      g_Config.fAspectRatioHackH = 1 / adjust;
    }
    else
    {
      // Hor+
      g_Config.fAspectRatioHackW = adjust;
      g_Config.fAspectRatioHackH = 1;
    }
  }
  else
  {
    // Hack is disabled.
    g_Config.fAspectRatioHackW = 1;
    g_Config.fAspectRatioHackH = 1;
  }

  // The rendering window size
  const float win_width = static_cast<float>(m_backbuffer_width);
  const float win_height = static_cast<float>(m_backbuffer_height);

  // FIXME: this breaks at very low widget sizes
  // Make ControllerInterface aware of the render window region actually being used
  // to adjust mouse cursor inputs.
  g_controller_interface.SetAspectRatioAdjustment(draw_aspect_ratio / (win_width / win_height));

  float draw_width = draw_aspect_ratio;
  float draw_height = 1;

  // Crop the picture to a standard aspect ratio. (if enabled)
  auto [crop_width, crop_height] = ApplyStandardAspectCrop(draw_width, draw_height);

  // scale the picture to fit the rendering window
  if (win_width / win_height >= crop_width / crop_height)
  {
    // the window is flatter than the picture
    draw_width *= win_height / crop_height;
    crop_width *= win_height / crop_height;
    draw_height *= win_height / crop_height;
    crop_height = win_height;
  }
  else
  {
    // the window is skinnier than the picture
    draw_width *= win_width / crop_width;
    draw_height *= win_width / crop_width;
    crop_height *= win_width / crop_width;
    crop_width = win_width;
  }

  // ensure divisibility by 4 to make it compatible with all the video encoders
  draw_width = std::ceil(draw_width) - static_cast<int>(std::ceil(draw_width)) % 4;
  draw_height = std::ceil(draw_height) - static_cast<int>(std::ceil(draw_height)) % 4;

  m_target_rectangle.left = static_cast<int>(std::round(win_width / 2.0 - draw_width / 2.0));
  m_target_rectangle.top = static_cast<int>(std::round(win_height / 2.0 - draw_height / 2.0));
  m_target_rectangle.right = m_target_rectangle.left + static_cast<int>(draw_width);
  m_target_rectangle.bottom = m_target_rectangle.top + static_cast<int>(draw_height);
}

void Renderer::SetWindowSize(int width, int height)
{
  const auto [out_width, out_height] = CalculateOutputDimensions(width, height);

  // Track the last values of width/height to avoid sending a window resize event every frame.
  if (out_width == m_last_window_request_width && out_height == m_last_window_request_height)
    return;

  m_last_window_request_width = out_width;
  m_last_window_request_height = out_height;
  Host_RequestRenderWindowSize(out_width, out_height);
}

// Crop to exactly 16:9 or 4:3 if enabled and not AspectMode::Stretch.
std::tuple<float, float> Renderer::ApplyStandardAspectCrop(float width, float height) const
{
  const auto aspect_mode = g_ActiveConfig.aspect_mode;

  if (!g_ActiveConfig.bCrop || aspect_mode == AspectMode::Stretch)
    return {width, height};

  // Force 4:3 or 16:9 by cropping the image.
  const float current_aspect = width / height;
  const float expected_aspect = (aspect_mode == AspectMode::AnalogWide ||
                                 (aspect_mode == AspectMode::Auto && m_is_game_widescreen)) ?
                                    (16.0f / 9.0f) :
                                    (4.0f / 3.0f);
  if (current_aspect > expected_aspect)
  {
    // keep height, crop width
    width = height * expected_aspect;
  }
  else
  {
    // keep width, crop height
    height = width / expected_aspect;
  }

  return {width, height};
}

std::tuple<int, int> Renderer::CalculateOutputDimensions(int width, int height) const
{
  width = std::max(width, 1);
  height = std::max(height, 1);

  auto [scaled_width, scaled_height] = ScaleToDisplayAspectRatio(width, height);

  // Apply crop if enabled.
  std::tie(scaled_width, scaled_height) = ApplyStandardAspectCrop(scaled_width, scaled_height);

  width = static_cast<int>(std::ceil(scaled_width));
  height = static_cast<int>(std::ceil(scaled_height));

  // UpdateDrawRectangle() makes sure that the rendered image is divisible by four for video
  // encoders, so do that here too to match it
  width -= width % 4;
  height -= height % 4;

  return std::make_tuple(width, height);
}

void Renderer::CheckFifoRecording()
{
  const bool was_recording = OpcodeDecoder::g_record_fifo_data;
  OpcodeDecoder::g_record_fifo_data = FifoRecorder::GetInstance().IsRecording();

  if (!OpcodeDecoder::g_record_fifo_data)
    return;

  if (!was_recording)
  {
    RecordVideoMemory();
  }

  auto& system = Core::System::GetInstance();
  auto& command_processor = system.GetCommandProcessor();
  const auto& fifo = command_processor.GetFifo();
  FifoRecorder::GetInstance().EndFrame(fifo.CPBase.load(std::memory_order_relaxed),
                                       fifo.CPEnd.load(std::memory_order_relaxed));
}

void Renderer::RecordVideoMemory()
{
  const u32* bpmem_ptr = reinterpret_cast<const u32*>(&bpmem);
  u32 cpmem[256] = {};
  // The FIFO recording format splits XF memory into xfmem and xfregs; follow
  // that split here.
  const u32* xfmem_ptr = reinterpret_cast<const u32*>(&xfmem);
  const u32* xfregs_ptr = reinterpret_cast<const u32*>(&xfmem) + FifoDataFile::XF_MEM_SIZE;
  u32 xfregs_size = sizeof(XFMemory) / 4 - FifoDataFile::XF_MEM_SIZE;

  g_main_cp_state.FillCPMemoryArray(cpmem);

  FifoRecorder::GetInstance().SetVideoMemory(bpmem_ptr, cpmem, xfmem_ptr, xfregs_ptr, xfregs_size,
                                             texMem);
}

bool Renderer::InitializeImGui()
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);

  if (!IMGUI_CHECKVERSION())
  {
    PanicAlertFmt("ImGui version check failed");
    return false;
  }
  if (!ImGui::CreateContext())
  {
    PanicAlertFmt("Creating ImGui context failed");
    return false;
  }

  // Don't create an ini file. TODO: Do we want this in the future?
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().DisplayFramebufferScale.x = m_backbuffer_scale;
  ImGui::GetIO().DisplayFramebufferScale.y = m_backbuffer_scale;
  ImGui::GetIO().FontGlobalScale = m_backbuffer_scale;
  ImGui::GetStyle().ScaleAllSizes(m_backbuffer_scale);
  ImGui::GetStyle().WindowRounding = 7.0f;

  PortableVertexDeclaration vdecl = {};
  vdecl.position = {ComponentFormat::Float, 2, offsetof(ImDrawVert, pos), true, false};
  vdecl.texcoords[0] = {ComponentFormat::Float, 2, offsetof(ImDrawVert, uv), true, false};
  vdecl.colors[0] = {ComponentFormat::UByte, 4, offsetof(ImDrawVert, col), true, false};
  vdecl.stride = sizeof(ImDrawVert);
  m_imgui_vertex_format = CreateNativeVertexFormat(vdecl);
  if (!m_imgui_vertex_format)
  {
    PanicAlertFmt("Failed to create ImGui vertex format");
    return false;
  }

  // Font texture(s).
  {
    ImGuiIO& io = ImGui::GetIO();
    u8* font_tex_pixels;
    int font_tex_width, font_tex_height;
    io.Fonts->GetTexDataAsRGBA32(&font_tex_pixels, &font_tex_width, &font_tex_height);

    TextureConfig font_tex_config(font_tex_width, font_tex_height, 1, 1, 1,
                                  AbstractTextureFormat::RGBA8, 0);
    std::unique_ptr<AbstractTexture> font_tex =
        CreateTexture(font_tex_config, "ImGui font texture");
    if (!font_tex)
    {
      PanicAlertFmt("Failed to create ImGui texture");
      return false;
    }
    font_tex->Load(0, font_tex_width, font_tex_height, font_tex_width, font_tex_pixels,
                   sizeof(u32) * font_tex_width * font_tex_height);

    io.Fonts->TexID = font_tex.get();

    m_imgui_textures.push_back(std::move(font_tex));
  }

  if (!RecompileImGuiPipeline())
    return false;

  m_imgui_last_frame_time = Common::Timer::NowUs();
  BeginImGuiFrameUnlocked();  // lock is already held
  return true;
}

bool Renderer::RecompileImGuiPipeline()
{
  std::unique_ptr<AbstractShader> vertex_shader =
      CreateShaderFromSource(ShaderStage::Vertex, FramebufferShaderGen::GenerateImGuiVertexShader(),
                             "ImGui vertex shader");
  std::unique_ptr<AbstractShader> pixel_shader = CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateImGuiPixelShader(), "ImGui pixel shader");
  if (!vertex_shader || !pixel_shader)
  {
    PanicAlertFmt("Failed to compile ImGui shaders");
    return false;
  }

  // GS is used to render the UI to both eyes in stereo modes.
  std::unique_ptr<AbstractShader> geometry_shader;
  if (UseGeometryShaderForUI())
  {
    geometry_shader = CreateShaderFromSource(
        ShaderStage::Geometry, FramebufferShaderGen::GeneratePassthroughGeometryShader(1, 1),
        "ImGui passthrough geometry shader");
    if (!geometry_shader)
    {
      PanicAlertFmt("Failed to compile ImGui geometry shader");
      return false;
    }
  }

  AbstractPipelineConfig pconfig = {};
  pconfig.vertex_format = m_imgui_vertex_format.get();
  pconfig.vertex_shader = vertex_shader.get();
  pconfig.geometry_shader = geometry_shader.get();
  pconfig.pixel_shader = pixel_shader.get();
  pconfig.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  pconfig.depth_state = RenderState::GetNoDepthTestingDepthState();
  pconfig.blending_state = RenderState::GetNoBlendingBlendState();
  pconfig.blending_state.blendenable = true;
  pconfig.blending_state.srcfactor = SrcBlendFactor::SrcAlpha;
  pconfig.blending_state.dstfactor = DstBlendFactor::InvSrcAlpha;
  pconfig.blending_state.srcfactoralpha = SrcBlendFactor::Zero;
  pconfig.blending_state.dstfactoralpha = DstBlendFactor::One;
  pconfig.framebuffer_state.color_texture_format = m_backbuffer_format;
  pconfig.framebuffer_state.depth_texture_format = AbstractTextureFormat::Undefined;
  pconfig.framebuffer_state.samples = 1;
  pconfig.framebuffer_state.per_sample_shading = false;
  pconfig.usage = AbstractPipelineUsage::Utility;
  m_imgui_pipeline = CreatePipeline(pconfig);
  if (!m_imgui_pipeline)
  {
    PanicAlertFmt("Failed to create imgui pipeline");
    return false;
  }

  return true;
}

void Renderer::ShutdownImGui()
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);

  ImGui::EndFrame();
  ImGui::DestroyContext();
  m_imgui_pipeline.reset();
  m_imgui_vertex_format.reset();
  m_imgui_textures.clear();
}

void Renderer::BeginImGuiFrame()
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);
  BeginImGuiFrameUnlocked();
}

void Renderer::BeginImGuiFrameUnlocked()
{
  const u64 current_time_us = Common::Timer::NowUs();
  const u64 time_diff_us = current_time_us - m_imgui_last_frame_time;
  const float time_diff_secs = static_cast<float>(time_diff_us / 1000000.0);
  m_imgui_last_frame_time = current_time_us;

  // Update I/O with window dimensions.
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize =
      ImVec2(static_cast<float>(m_backbuffer_width), static_cast<float>(m_backbuffer_height));
  io.DeltaTime = time_diff_secs;

  ImGui::NewFrame();
}

void Renderer::DrawImGui()
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  if (!draw_data)
    return;

  SetViewport(0.0f, 0.0f, static_cast<float>(m_backbuffer_width),
              static_cast<float>(m_backbuffer_height), 0.0f, 1.0f);

  // Uniform buffer for draws.
  struct ImGuiUbo
  {
    float u_rcp_viewport_size_mul2[2];
    float padding[2];
  };
  ImGuiUbo ubo = {{1.0f / m_backbuffer_width * 2.0f, 1.0f / m_backbuffer_height * 2.0f}};

  // Set up common state for drawing.
  SetPipeline(m_imgui_pipeline.get());
  SetSamplerState(0, RenderState::GetPointSamplerState());
  g_vertex_manager->UploadUtilityUniforms(&ubo, sizeof(ubo));

  for (int i = 0; i < draw_data->CmdListsCount; i++)
  {
    const ImDrawList* cmdlist = draw_data->CmdLists[i];
    if (cmdlist->VtxBuffer.empty() || cmdlist->IdxBuffer.empty())
      return;

    u32 base_vertex, base_index;
    g_vertex_manager->UploadUtilityVertices(cmdlist->VtxBuffer.Data, sizeof(ImDrawVert),
                                            cmdlist->VtxBuffer.Size, cmdlist->IdxBuffer.Data,
                                            cmdlist->IdxBuffer.Size, &base_vertex, &base_index);

    for (const ImDrawCmd& cmd : cmdlist->CmdBuffer)
    {
      if (cmd.UserCallback)
      {
        cmd.UserCallback(cmdlist, &cmd);
        continue;
      }

      SetScissorRect(ConvertFramebufferRectangle(
          MathUtil::Rectangle<int>(
              static_cast<int>(cmd.ClipRect.x), static_cast<int>(cmd.ClipRect.y),
              static_cast<int>(cmd.ClipRect.z), static_cast<int>(cmd.ClipRect.w)),
          m_current_framebuffer));
      SetTexture(0, reinterpret_cast<const AbstractTexture*>(cmd.TextureId));
      DrawIndexed(base_index, cmd.ElemCount, base_vertex);
      base_index += cmd.ElemCount;
    }
  }

  // Some capture software (such as OBS) hooks SwapBuffers and uses glBlitFramebuffer to copy our
  // back buffer just before swap. Because glBlitFramebuffer honors the scissor test, the capture
  // itself will be clipped to whatever bounds were last set by ImGui, resulting in a rather useless
  // capture whenever any ImGui windows are open. We'll reset the scissor rectangle to the entire
  // viewport here to avoid this problem.
  SetScissorRect(ConvertFramebufferRectangle(
      MathUtil::Rectangle<int>(0, 0, m_backbuffer_width, m_backbuffer_height),
      m_current_framebuffer));
}

bool Renderer::UseGeometryShaderForUI() const
{
  // OpenGL doesn't render to a 2-layer backbuffer like D3D/Vulkan for quad-buffered stereo,
  // instead drawing twice and the eye selected by glDrawBuffer() (see
  // OGL::Renderer::RenderXFBToScreen).
  return g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer &&
         g_ActiveConfig.backend_info.api_type != APIType::OpenGL;
}

std::unique_lock<std::mutex> Renderer::GetImGuiLock()
{
  return std::unique_lock<std::mutex>(m_imgui_mutex);
}

void Renderer::BeginUIFrame()
{
  if (IsHeadless())
    return;

  BeginUtilityDrawing();
  BindBackbuffer({0.0f, 0.0f, 0.0f, 1.0f});
}

void Renderer::EndUIFrame()
{
  {
    auto lock = GetImGuiLock();
    ImGui::Render();
  }

  if (!IsHeadless())
  {
    DrawImGui();

    std::lock_guard<std::mutex> guard(m_swap_mutex);
    PresentBackbuffer();
    EndUtilityDrawing();
  }

  BeginImGuiFrame();
}

void Renderer::ForceReloadTextures()
{
  m_force_reload_textures.Set();
}

// Heuristic to detect if a GameCube game is in 16:9 anamorphic widescreen mode.
void Renderer::UpdateWidescreenHeuristic()
{
  // VertexManager maintains no statistics in Wii mode.
  if (SConfig::GetInstance().bWii)
    return;

  const auto flush_statistics = g_vertex_manager->ResetFlushAspectRatioCount();

  // If suggested_aspect_mode (GameINI) is configured don't use heuristic.
  if (g_ActiveConfig.suggested_aspect_mode != AspectMode::Auto)
    return;

  // If widescreen hack isn't active and aspect_mode (UI) is 4:3 or 16:9 don't use heuristic.
  if (!g_ActiveConfig.bWidescreenHack && (g_ActiveConfig.aspect_mode == AspectMode::Analog ||
                                          g_ActiveConfig.aspect_mode == AspectMode::AnalogWide))
    return;

  // Modify the threshold based on which aspect ratio we're already using:
  // If the game's in 4:3, it probably won't switch to anamorphic, and vice-versa.
  static constexpr u32 TRANSITION_THRESHOLD = 3;

  const auto looks_normal = [](auto& counts) {
    return counts.normal_vertex_count > counts.anamorphic_vertex_count * TRANSITION_THRESHOLD;
  };
  const auto looks_anamorphic = [](auto& counts) {
    return counts.anamorphic_vertex_count > counts.normal_vertex_count * TRANSITION_THRESHOLD;
  };

  const auto& persp = flush_statistics.perspective;
  const auto& ortho = flush_statistics.orthographic;

  const auto ortho_looks_anamorphic = looks_anamorphic(ortho);

  if (looks_anamorphic(persp) || ortho_looks_anamorphic)
  {
    // If either perspective or orthographic projections look anamorphic, it's a safe bet.
    m_is_game_widescreen = true;
  }
  else if (looks_normal(persp) || (m_was_orthographically_anamorphic && looks_normal(ortho)))
  {
    // Many widescreen games (or AR/GeckoCodes) use anamorphic perspective projections
    // with NON-anamorphic orthographic projections.
    // This can cause incorrect changes to 4:3 when perspective projections are temporarily not
    // shown. e.g. Animal Crossing's inventory menu.
    // Unless we were in a situation which was orthographically anamorphic
    // we won't consider orthographic data for changes from 16:9 to 4:3.
    m_is_game_widescreen = false;
  }

  m_was_orthographically_anamorphic = ortho_looks_anamorphic;
}

void Renderer::Swap(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks)
{
  if (SConfig::GetInstance().bWii)
    m_is_game_widescreen = Config::Get(Config::SYSCONF_WIDESCREEN);

  // suggested_aspect_mode overrides SYSCONF_WIDESCREEN
  if (g_ActiveConfig.suggested_aspect_mode == AspectMode::Analog)
    m_is_game_widescreen = false;
  else if (g_ActiveConfig.suggested_aspect_mode == AspectMode::AnalogWide)
    m_is_game_widescreen = true;

  // If widescreen hack is disabled override game's AR if UI is set to 4:3 or 16:9.
  if (!g_ActiveConfig.bWidescreenHack)
  {
    const auto aspect_mode = g_ActiveConfig.aspect_mode;
    if (aspect_mode == AspectMode::Analog)
      m_is_game_widescreen = false;
    else if (aspect_mode == AspectMode::AnalogWide)
      m_is_game_widescreen = true;
  }

  // Ensure the last frame was written to the dump.
  // This is required even if frame dumping has stopped, since the frame dump is one frame
  // behind the renderer.
  FlushFrameDump();

  if (g_ActiveConfig.bGraphicMods)
  {
    m_graphics_mod_manager.EndOfFrame();
  }

  g_framebuffer_manager->EndOfFrame();

  if (xfb_addr && fb_width && fb_stride && fb_height)
  {
    // Get the current XFB from texture cache
    MathUtil::Rectangle<int> xfb_rect;
    const auto* xfb_entry =
        g_texture_cache->GetXFBTexture(xfb_addr, fb_width, fb_height, fb_stride, &xfb_rect);
    const bool is_duplicate_frame = xfb_entry->id == m_last_xfb_id;

    m_vps_counter.Update();
    if (!is_duplicate_frame)
      m_fps_counter.Update();

    if (xfb_entry && (!g_ActiveConfig.bSkipPresentingDuplicateXFBs || !is_duplicate_frame))
    {
      m_last_xfb_id = xfb_entry->id;

      // Since we use the common pipelines here and draw vertices if a batch is currently being
      // built by the vertex loader, we end up trampling over its pointer, as we share the buffer
      // with the loader, and it has not been unmapped yet. Force a pipeline flush to avoid this.
      g_vertex_manager->Flush();

      // Render any UI elements to the draw list.
      {
        auto lock = GetImGuiLock();

        DrawDebugText();
        OSD::DrawMessages();
        ImGui::Render();
      }

      // Render the XFB to the screen.
      BeginUtilityDrawing();
      if (!IsHeadless())
      {
        BindBackbuffer({{0.0f, 0.0f, 0.0f, 1.0f}});

        if (!is_duplicate_frame)
          UpdateWidescreenHeuristic();

        UpdateDrawRectangle();

        // Adjust the source rectangle instead of using an oversized viewport to render the XFB.
        auto render_target_rc = GetTargetRectangle();
        auto render_source_rc = xfb_rect;
        AdjustRectanglesToFitBounds(&render_target_rc, &render_source_rc, m_backbuffer_width,
                                    m_backbuffer_height);
        RenderXFBToScreen(render_target_rc, xfb_entry->texture.get(), render_source_rc);

        DrawImGui();

        // Present to the window system.
        {
          std::lock_guard<std::mutex> guard(m_swap_mutex);
          PresentBackbuffer();
        }

        // Update the window size based on the frame that was just rendered.
        // Due to depending on guest state, we need to call this every frame.
        SetWindowSize(xfb_rect.GetWidth(), xfb_rect.GetHeight());
      }

      if (!is_duplicate_frame)
      {
        DolphinAnalytics::PerformanceSample perf_sample;
        perf_sample.speed_ratio = SystemTimers::GetEstimatedEmulationPerformance();
        perf_sample.num_prims = g_stats.this_frame.num_prims + g_stats.this_frame.num_dl_prims;
        perf_sample.num_draw_calls = g_stats.this_frame.num_draw_calls;
        DolphinAnalytics::Instance().ReportPerformanceInfo(std::move(perf_sample));

        if (IsFrameDumping())
          DumpCurrentFrame(xfb_entry->texture.get(), xfb_rect, ticks, m_frame_count);

        // Begin new frame
        m_frame_count++;
        g_stats.ResetFrame();
      }

      g_shader_cache->RetrieveAsyncShaders();
      g_vertex_manager->OnEndFrame();
      BeginImGuiFrame();

      // We invalidate the pipeline object at the start of the frame.
      // This is for the rare case where only a single pipeline configuration is used,
      // and hybrid ubershaders have compiled the specialized shader, but without any
      // state changes the specialized shader will not take over.
      g_vertex_manager->InvalidatePipelineObject();

      if (m_force_reload_textures.TestAndClear())
      {
        g_texture_cache->ForceReload();
      }
      else
      {
        // Flush any outstanding EFB copies to RAM, in case the game is running at an uncapped frame
        // rate and not waiting for vblank. Otherwise, we'd end up with a huge list of pending
        // copies.
        g_texture_cache->FlushEFBCopies();
      }

      if (!is_duplicate_frame)
      {
        // Remove stale EFB/XFB copies.
        g_texture_cache->Cleanup(m_frame_count);
        const double last_speed_denominator =
            m_fps_counter.GetDeltaTime() * VideoInterface::GetTargetRefreshRate();
        // The denominator should always be > 0 but if it's not, just return 1
        const double last_speed =
            last_speed_denominator > 0.0 ? (1.0 / last_speed_denominator) : 1.0;
        Core::Callback_FramePresented(last_speed);
      }

      // Handle any config changes, this gets propagated to the backend.
      CheckForConfigChanges();
      g_Config.iSaveTargetId = 0;

      EndUtilityDrawing();
    }
    else
    {
      Flush();
    }

    // Update our last xfb values
    m_last_xfb_addr = xfb_addr;
    m_last_xfb_ticks = ticks;
    m_last_xfb_width = fb_width;
    m_last_xfb_stride = fb_stride;
    m_last_xfb_height = fb_height;
  }
  else
  {
    Flush();
  }
}

void Renderer::RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                                 const AbstractTexture* source_texture,
                                 const MathUtil::Rectangle<int>& source_rc)
{
  if (g_ActiveConfig.stereo_mode == StereoMode::SBS ||
      g_ActiveConfig.stereo_mode == StereoMode::TAB)
  {
    const auto [left_rc, right_rc] = ConvertStereoRectangle(target_rc);

    m_post_processor->BlitFromTexture(left_rc, source_rc, source_texture, 0);
    m_post_processor->BlitFromTexture(right_rc, source_rc, source_texture, 1);
  }
  else
  {
    m_post_processor->BlitFromTexture(target_rc, source_rc, source_texture, 0);
  }
}

bool Renderer::IsFrameDumping() const
{
  if (m_screenshot_request.IsSet())
    return true;

  if (Config::Get(Config::MAIN_MOVIE_DUMP_FRAMES))
    return true;

  return false;
}

void Renderer::DumpCurrentFrame(const AbstractTexture* src_texture,
                                const MathUtil::Rectangle<int>& src_rect, u64 ticks,
                                int frame_number)
{
  int source_width = src_rect.GetWidth();
  int source_height = src_rect.GetHeight();
  int target_width, target_height;
  if (!g_ActiveConfig.bInternalResolutionFrameDumps && !IsHeadless())
  {
    auto target_rect = GetTargetRectangle();
    target_width = target_rect.GetWidth();
    target_height = target_rect.GetHeight();
  }
  else
  {
    std::tie(target_width, target_height) = CalculateOutputDimensions(source_width, source_height);
  }

  // We only need to render a copy if we need to stretch/scale the XFB copy.
  MathUtil::Rectangle<int> copy_rect = src_rect;
  if (source_width != target_width || source_height != target_height)
  {
    if (!CheckFrameDumpRenderTexture(target_width, target_height))
      return;

    ScaleTexture(m_frame_dump_render_framebuffer.get(), m_frame_dump_render_framebuffer->GetRect(),
                 src_texture, src_rect);
    src_texture = m_frame_dump_render_texture.get();
    copy_rect = src_texture->GetRect();
  }

  if (!CheckFrameDumpReadbackTexture(target_width, target_height))
    return;

  m_frame_dump_readback_texture->CopyFromTexture(src_texture, copy_rect, 0, 0,
                                                 m_frame_dump_readback_texture->GetRect());
  m_last_frame_state = m_frame_dump.FetchState(ticks, frame_number);
  m_frame_dump_needs_flush = true;
}

bool Renderer::CheckFrameDumpRenderTexture(u32 target_width, u32 target_height)
{
  // Ensure framebuffer exists (we lazily allocate it in case frame dumping isn't used).
  // Or, resize texture if it isn't large enough to accommodate the current frame.
  if (m_frame_dump_render_texture && m_frame_dump_render_texture->GetWidth() == target_width &&
      m_frame_dump_render_texture->GetHeight() == target_height)
  {
    return true;
  }

  // Recreate texture, but release before creating so we don't temporarily use twice the RAM.
  m_frame_dump_render_framebuffer.reset();
  m_frame_dump_render_texture.reset();
  m_frame_dump_render_texture =
      CreateTexture(TextureConfig(target_width, target_height, 1, 1, 1,
                                  AbstractTextureFormat::RGBA8, AbstractTextureFlag_RenderTarget),
                    "Frame dump render texture");
  if (!m_frame_dump_render_texture)
  {
    PanicAlertFmt("Failed to allocate frame dump render texture");
    return false;
  }
  m_frame_dump_render_framebuffer = CreateFramebuffer(m_frame_dump_render_texture.get(), nullptr);
  ASSERT(m_frame_dump_render_framebuffer);
  return true;
}

bool Renderer::CheckFrameDumpReadbackTexture(u32 target_width, u32 target_height)
{
  std::unique_ptr<AbstractStagingTexture>& rbtex = m_frame_dump_readback_texture;
  if (rbtex && rbtex->GetWidth() == target_width && rbtex->GetHeight() == target_height)
    return true;

  rbtex.reset();
  rbtex = CreateStagingTexture(
      StagingTextureType::Readback,
      TextureConfig(target_width, target_height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0));
  if (!rbtex)
    return false;

  return true;
}

void Renderer::FlushFrameDump()
{
  if (!m_frame_dump_needs_flush)
    return;

  // Ensure dumping thread is done with output texture before swapping.
  FinishFrameData();

  std::swap(m_frame_dump_output_texture, m_frame_dump_readback_texture);

  // Queue encoding of the last frame dumped.
  auto& output = m_frame_dump_output_texture;
  output->Flush();
  if (output->Map())
  {
    DumpFrameData(reinterpret_cast<u8*>(output->GetMappedPointer()), output->GetConfig().width,
                  output->GetConfig().height, static_cast<int>(output->GetMappedStride()));
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Failed to map texture for dumping.");
  }

  m_frame_dump_needs_flush = false;

  // Shutdown frame dumping if it is no longer active.
  if (!IsFrameDumping())
    ShutdownFrameDumping();
}

void Renderer::ShutdownFrameDumping()
{
  // Ensure the last queued readback has been sent to the encoder.
  FlushFrameDump();

  if (!m_frame_dump_thread_running.IsSet())
    return;

  // Ensure previous frame has been encoded.
  FinishFrameData();

  // Wake thread up, and wait for it to exit.
  m_frame_dump_thread_running.Clear();
  m_frame_dump_start.Set();
  if (m_frame_dump_thread.joinable())
    m_frame_dump_thread.join();
  m_frame_dump_render_framebuffer.reset();
  m_frame_dump_render_texture.reset();

  m_frame_dump_readback_texture.reset();
  m_frame_dump_output_texture.reset();
}

void Renderer::DumpFrameData(const u8* data, int w, int h, int stride)
{
  m_frame_dump_data = FrameDump::FrameData{data, w, h, stride, m_last_frame_state};

  if (!m_frame_dump_thread_running.IsSet())
  {
    if (m_frame_dump_thread.joinable())
      m_frame_dump_thread.join();
    m_frame_dump_thread_running.Set();
    m_frame_dump_thread = std::thread(&Renderer::FrameDumpThreadFunc, this);
  }

  // Wake worker thread up.
  m_frame_dump_start.Set();
  m_frame_dump_frame_running = true;
}

void Renderer::FinishFrameData()
{
  if (!m_frame_dump_frame_running)
    return;

  m_frame_dump_done.Wait();
  m_frame_dump_frame_running = false;

  m_frame_dump_output_texture->Unmap();
}

void Renderer::FrameDumpThreadFunc()
{
  Common::SetCurrentThreadName("FrameDumping");

  bool dump_to_ffmpeg = !g_ActiveConfig.bDumpFramesAsImages;
  bool frame_dump_started = false;

// If Dolphin was compiled without ffmpeg, we only support dumping to images.
#if !defined(HAVE_FFMPEG)
  if (dump_to_ffmpeg)
  {
    WARN_LOG_FMT(VIDEO, "FrameDump: Dolphin was not compiled with FFmpeg, using fallback option. "
                        "Frames will be saved as PNG images instead.");
    dump_to_ffmpeg = false;
  }
#endif

  while (true)
  {
    m_frame_dump_start.Wait();
    if (!m_frame_dump_thread_running.IsSet())
      break;

    auto frame = m_frame_dump_data;

    // Save screenshot
    if (m_screenshot_request.TestAndClear())
    {
      std::lock_guard<std::mutex> lk(m_screenshot_lock);

      if (DumpFrameToPNG(frame, m_screenshot_name))
        OSD::AddMessage("Screenshot saved to " + m_screenshot_name);

      // Reset settings
      m_screenshot_name.clear();
      m_screenshot_completed.Set();
    }

    if (Config::Get(Config::MAIN_MOVIE_DUMP_FRAMES))
    {
      if (!frame_dump_started)
      {
        if (dump_to_ffmpeg)
          frame_dump_started = StartFrameDumpToFFMPEG(frame);
        else
          frame_dump_started = StartFrameDumpToImage(frame);

        // Stop frame dumping if we fail to start.
        if (!frame_dump_started)
          Config::SetCurrent(Config::MAIN_MOVIE_DUMP_FRAMES, false);
      }

      // If we failed to start frame dumping, don't write a frame.
      if (frame_dump_started)
      {
        if (dump_to_ffmpeg)
          DumpFrameToFFMPEG(frame);
        else
          DumpFrameToImage(frame);
      }
    }

    m_frame_dump_done.Set();
  }

  if (frame_dump_started)
  {
    // No additional cleanup is needed when dumping to images.
    if (dump_to_ffmpeg)
      StopFrameDumpToFFMPEG();
  }
}

#if defined(HAVE_FFMPEG)

bool Renderer::StartFrameDumpToFFMPEG(const FrameDump::FrameData& frame)
{
  // If dumping started at boot, the start time must be set to the boot time to maintain audio sync.
  // TODO: Perhaps we should care about this when starting dumping in the middle of emulation too,
  // but it's less important there since the first frame to dump usually gets delivered quickly.
  const u64 start_ticks = frame.state.frame_number == 0 ? 0 : frame.state.ticks;
  return m_frame_dump.Start(frame.width, frame.height, start_ticks);
}

void Renderer::DumpFrameToFFMPEG(const FrameDump::FrameData& frame)
{
  m_frame_dump.AddFrame(frame);
}

void Renderer::StopFrameDumpToFFMPEG()
{
  m_frame_dump.Stop();
}

#else

bool Renderer::StartFrameDumpToFFMPEG(const FrameDump::FrameData&)
{
  return false;
}

void Renderer::DumpFrameToFFMPEG(const FrameDump::FrameData&)
{
}

void Renderer::StopFrameDumpToFFMPEG()
{
}

#endif  // defined(HAVE_FFMPEG)

std::string Renderer::GetFrameDumpNextImageFileName() const
{
  return fmt::format("{}framedump_{}.png", File::GetUserPath(D_DUMPFRAMES_IDX),
                     m_frame_dump_image_counter);
}

bool Renderer::StartFrameDumpToImage(const FrameDump::FrameData&)
{
  m_frame_dump_image_counter = 1;
  if (!Config::Get(Config::MAIN_MOVIE_DUMP_FRAMES_SILENT))
  {
    // Only check for the presence of the first image to confirm overwriting.
    // A previous run will always have at least one image, and it's safe to assume that if the user
    // has allowed the first image to be overwritten, this will apply any remaining images as well.
    std::string filename = GetFrameDumpNextImageFileName();
    if (File::Exists(filename))
    {
      if (!AskYesNoFmtT("Frame dump image(s) '{0}' already exists. Overwrite?", filename))
        return false;
    }
  }

  return true;
}

void Renderer::DumpFrameToImage(const FrameDump::FrameData& frame)
{
  DumpFrameToPNG(frame, GetFrameDumpNextImageFileName());
  m_frame_dump_image_counter++;
}

bool Renderer::UseVertexDepthRange() const
{
  // We can't compute the depth range in the vertex shader if we don't support depth clamp.
  if (!g_ActiveConfig.backend_info.bSupportsDepthClamp)
    return false;

  // We need a full depth range if a ztexture is used.
  if (bpmem.ztex2.op != ZTexOp::Disabled && !bpmem.zcontrol.early_ztest)
    return true;

  // If an inverted depth range is unsupported, we also need to check if the range is inverted.
  if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange && xfmem.viewport.zRange < 0.0f)
    return true;

  // If an oversized depth range or a ztexture is used, we need to calculate the depth range
  // in the vertex shader.
  return fabs(xfmem.viewport.zRange) > 16777215.0f || fabs(xfmem.viewport.farZ) > 16777215.0f;
}

void Renderer::DoState(PointerWrap& p)
{
  p.Do(m_is_game_widescreen);
  p.Do(m_frame_count);
  p.Do(m_prev_efb_format);
  p.Do(m_last_xfb_ticks);
  p.Do(m_last_xfb_addr);
  p.Do(m_last_xfb_width);
  p.Do(m_last_xfb_stride);
  p.Do(m_last_xfb_height);
  p.DoArray(m_bounding_box_fallback);

  m_bounding_box->DoState(p);

  if (p.IsReadMode())
  {
    // Force the next xfb to be displayed.
    m_last_xfb_id = std::numeric_limits<u64>::max();

    m_was_orthographically_anamorphic = false;

    // And actually display it.
    Swap(m_last_xfb_addr, m_last_xfb_width, m_last_xfb_stride, m_last_xfb_height, m_last_xfb_ticks);
  }

#if defined(HAVE_FFMPEG)
  m_frame_dump.DoState(p);
#endif
}

std::unique_ptr<VideoCommon::AsyncShaderCompiler> Renderer::CreateAsyncShaderCompiler()
{
  return std::make_unique<VideoCommon::AsyncShaderCompiler>();
}

const GraphicsModManager& Renderer::GetGraphicsModManager() const
{
  return m_graphics_mod_manager;
}
