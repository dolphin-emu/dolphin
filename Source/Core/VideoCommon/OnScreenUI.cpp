// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/OnScreenUI.h"

#include "Common/EnumMap.h"
#include "Common/Profiler.h"
#include "Common/Timer.h"

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

#include <inttypes.h>
#include <mutex>

#include <imgui.h>
#include <implot.h>

namespace VideoCommon
{
bool OnScreenUI::Initialize(u32 width, u32 height, float scale)
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
  if (!ImPlot::CreateContext())
  {
    PanicAlertFmt("Creating ImPlot context failed");
    return false;
  }

  // Don't create an ini file. TODO: Do we want this in the future?
  ImGui::GetIO().IniFilename = nullptr;
  SetScale(scale);

  PortableVertexDeclaration vdecl = {};
  vdecl.position = {ComponentFormat::Float, 2, offsetof(ImDrawVert, pos), true, false};
  vdecl.texcoords[0] = {ComponentFormat::Float, 2, offsetof(ImDrawVert, uv), true, false};
  vdecl.colors[0] = {ComponentFormat::UByte, 4, offsetof(ImDrawVert, col), true, false};
  vdecl.stride = sizeof(ImDrawVert);
  m_imgui_vertex_format = g_gfx->CreateNativeVertexFormat(vdecl);
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
                                  AbstractTextureFormat::RGBA8, 0,
                                  AbstractTextureType::Texture_2DArray);
    std::unique_ptr<AbstractTexture> font_tex =
        g_gfx->CreateTexture(font_tex_config, "ImGui font texture");
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
  m_ready = true;
  BeginImGuiFrameUnlocked(width, height);  // lock is already held

  return true;
}

OnScreenUI::~OnScreenUI()
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);

  ImGui::EndFrame();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
}

bool OnScreenUI::RecompileImGuiPipeline()
{
  if (g_presenter->GetBackbufferFormat() == AbstractTextureFormat::Undefined)
  {
    // No backbuffer (nogui) means no imgui rendering will happen
    // Some backends don't like making pipelines with no render targets
    return true;
  }

  const bool linear_space_output =
      g_presenter->GetBackbufferFormat() == AbstractTextureFormat::RGBA16F;

  std::unique_ptr<AbstractShader> vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateImGuiVertexShader(),
      "ImGui vertex shader");
  std::unique_ptr<AbstractShader> pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateImGuiPixelShader(linear_space_output),
      "ImGui pixel shader");
  if (!vertex_shader || !pixel_shader)
  {
    PanicAlertFmt("Failed to compile ImGui shaders");
    return false;
  }

  // GS is used to render the UI to both eyes in stereo modes.
  std::unique_ptr<AbstractShader> geometry_shader;
  if (g_gfx->UseGeometryShaderForUI())
  {
    geometry_shader = g_gfx->CreateShaderFromSource(
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
  pconfig.framebuffer_state.color_texture_format = g_presenter->GetBackbufferFormat();
  pconfig.framebuffer_state.depth_texture_format = AbstractTextureFormat::Undefined;
  pconfig.framebuffer_state.samples = 1;
  pconfig.framebuffer_state.per_sample_shading = false;
  pconfig.usage = AbstractPipelineUsage::Utility;
  m_imgui_pipeline = g_gfx->CreatePipeline(pconfig);
  if (!m_imgui_pipeline)
  {
    PanicAlertFmt("Failed to create imgui pipeline");
    return false;
  }

  return true;
}

void OnScreenUI::BeginImGuiFrame(u32 width, u32 height)
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);
  BeginImGuiFrameUnlocked(width, height);
}

void OnScreenUI::BeginImGuiFrameUnlocked(u32 width, u32 height)
{
  m_backbuffer_width = width;
  m_backbuffer_height = height;

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

void OnScreenUI::DrawImGui()
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  if (!draw_data)
    return;

  g_gfx->SetViewport(0.0f, 0.0f, static_cast<float>(m_backbuffer_width),
                     static_cast<float>(m_backbuffer_height), 0.0f, 1.0f);

  // Uniform buffer for draws.
  struct ImGuiUbo
  {
    float u_rcp_viewport_size_mul2[2];
    float padding[2];
  };
  ImGuiUbo ubo = {{1.0f / m_backbuffer_width * 2.0f, 1.0f / m_backbuffer_height * 2.0f}};

  // Set up common state for drawing.
  g_gfx->SetPipeline(m_imgui_pipeline.get());
  g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());
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

      g_gfx->SetScissorRect(g_gfx->ConvertFramebufferRectangle(
          MathUtil::Rectangle<int>(
              static_cast<int>(cmd.ClipRect.x), static_cast<int>(cmd.ClipRect.y),
              static_cast<int>(cmd.ClipRect.z), static_cast<int>(cmd.ClipRect.w)),
          g_gfx->GetCurrentFramebuffer()));
      g_gfx->SetTexture(0, reinterpret_cast<const AbstractTexture*>(cmd.TextureId));
      g_gfx->DrawIndexed(base_index, cmd.ElemCount, base_vertex);
      base_index += cmd.ElemCount;
    }
  }

  // Some capture software (such as OBS) hooks SwapBuffers and uses glBlitFramebuffer to copy our
  // back buffer just before swap. Because glBlitFramebuffer honors the scissor test, the capture
  // itself will be clipped to whatever bounds were last set by ImGui, resulting in a rather useless
  // capture whenever any ImGui windows are open. We'll reset the scissor rectangle to the entire
  // viewport here to avoid this problem.
  g_gfx->SetScissorRect(g_gfx->ConvertFramebufferRectangle(
      MathUtil::Rectangle<int>(0, 0, m_backbuffer_width, m_backbuffer_height),
      g_gfx->GetCurrentFramebuffer()));
}

// Create On-Screen-Messages
void OnScreenUI::DrawDebugText()
{
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
      auto& movie = Core::System::GetInstance().GetMovie();
      if (movie.IsPlayingInput())
      {
        ImGui::Text("Frame: %" PRIu64 " / %" PRIu64, movie.GetCurrentFrame(),
                    movie.GetTotalFrames());
        ImGui::Text("Input: %" PRIu64 " / %" PRIu64, movie.GetCurrentInputCount(),
                    movie.GetTotalInputCount());
      }
      else if (Config::Get(Config::MAIN_SHOW_FRAME_COUNT))
      {
        ImGui::Text("Frame: %" PRIu64, movie.GetCurrentFrame());
        if (movie.IsRecordingInput())
          ImGui::Text("Input: %" PRIu64, movie.GetCurrentInputCount());
      }
      if (Config::Get(Config::MAIN_SHOW_LAG))
        ImGui::Text("Lag: %" PRIu64 "\n", movie.GetCurrentLagCount());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY))
        ImGui::TextUnformatted(movie.GetInputDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RTC))
        ImGui::TextUnformatted(movie.GetRTCDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RERECORD))
        ImGui::TextUnformatted(movie.GetRerecords().c_str());
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

void OnScreenUI::DrawChallengesAndLeaderboards()
{
  if (!Config::Get(Config::MAIN_OSD_MESSAGES))
    return;
#ifdef USE_RETRO_ACHIEVEMENTS
  auto& instance = AchievementManager::GetInstance();
  std::lock_guard lg{instance.GetLock()};
  if (instance.AreChallengesUpdated())
  {
    instance.ResetChallengesUpdated();
    const auto& challenges = instance.GetActiveChallenges();
    m_challenge_texture_map.clear();
    for (const auto& name : challenges)
    {
      const auto& icon = instance.GetAchievementBadge(name, false);
      const u32 width = icon.width;
      const u32 height = icon.height;
      TextureConfig tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                               AbstractTextureType::Texture_2DArray);
      auto res = m_challenge_texture_map.insert_or_assign(name, g_gfx->CreateTexture(tex_config));
      res.first->second->Load(0, width, height, width, icon.data.data(),
                              sizeof(u32) * width * height);
    }
  }

  float leaderboard_y = ImGui::GetIO().DisplaySize.y;
  if (!m_challenge_texture_map.empty())
  {
    float scale = ImGui::GetIO().DisplaySize.y / 1024.0;
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), 0,
                            ImVec2(1, 1));
    if (ImGui::Begin("Challenges", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      for (auto& [name, texture] : m_challenge_texture_map)
      {
        ImGui::Image(texture.get(), ImVec2(static_cast<float>(texture->GetWidth()) * scale,
                                           static_cast<float>(texture->GetHeight()) * scale));
        ImGui::SameLine();
      }
    }
    leaderboard_y -= ImGui::GetWindowHeight();
    ImGui::End();
  }

  const auto& leaderboard_progress = instance.GetActiveLeaderboards();
  if (!leaderboard_progress.empty())
  {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, leaderboard_y), 0,
                            ImVec2(1.0, 1.0));
    ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Leaderboards", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      for (const auto& value : leaderboard_progress)
        ImGui::TextUnformatted(value.c_str());
    }
    ImGui::End();
  }
#endif  // USE_RETRO_ACHIEVEMENTS
}

void OnScreenUI::Finalize()
{
  auto lock = GetImGuiLock();

  g_perf_metrics.DrawImGuiStats(m_backbuffer_scale);
  DrawDebugText();
  OSD::DrawMessages();
  DrawChallengesAndLeaderboards();
  ImGui::Render();
}

std::unique_lock<std::mutex> OnScreenUI::GetImGuiLock()
{
  return std::unique_lock<std::mutex>(m_imgui_mutex);
}

void OnScreenUI::SetScale(float backbuffer_scale)
{
  ImGui::GetIO().DisplayFramebufferScale.x = backbuffer_scale;
  ImGui::GetIO().DisplayFramebufferScale.y = backbuffer_scale;
  ImGui::GetIO().FontGlobalScale = backbuffer_scale;
  // ScaleAllSizes scales in-place, so calling it twice will double-apply the scale
  // Reset the style first so that the scale is applied to the base style, not an already-scaled one
  ImGui::GetStyle() = {};
  ImGui::GetStyle().WindowRounding = 7.0f;
  ImGui::GetStyle().ScaleAllSizes(backbuffer_scale);

  m_backbuffer_scale = backbuffer_scale;
}
void OnScreenUI::SetKeyMap(const DolphinKeyMap& key_map)
{
  static constexpr DolphinKeyMap dolphin_to_imgui_map = {
      ImGuiKey_Tab,       ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow,
      ImGuiKey_DownArrow, ImGuiKey_PageUp,    ImGuiKey_PageDown,   ImGuiKey_Home,
      ImGuiKey_End,       ImGuiKey_Insert,    ImGuiKey_Delete,     ImGuiKey_Backspace,
      ImGuiKey_Space,     ImGuiKey_Enter,     ImGuiKey_Escape,     ImGuiKey_KeypadEnter,
      ImGuiKey_A,         ImGuiKey_C,         ImGuiKey_V,          ImGuiKey_X,
      ImGuiKey_Y,         ImGuiKey_Z,
  };

  auto lock = GetImGuiLock();

  if (!ImGui::GetCurrentContext())
    return;

  m_dolphin_to_imgui_map.clear();
  for (int dolphin_key = 0; dolphin_key <= static_cast<int>(DolphinKey::Z); dolphin_key++)
  {
    const int imgui_key = dolphin_to_imgui_map[DolphinKey(dolphin_key)];
    if (imgui_key >= 0)
    {
      const int mapped_key = key_map[DolphinKey(dolphin_key)];
      m_dolphin_to_imgui_map[mapped_key & 0x1FF] = imgui_key;
    }
  }
}

void OnScreenUI::SetKey(u32 key, bool is_down, const char* chars)
{
  auto lock = GetImGuiLock();
  if (auto iter = m_dolphin_to_imgui_map.find(key); iter != m_dolphin_to_imgui_map.end())
    ImGui::GetIO().AddKeyEvent((ImGuiKey)iter->second, is_down);

  if (chars)
    ImGui::GetIO().AddInputCharactersUTF8(chars);
}

void OnScreenUI::SetMousePos(float x, float y)
{
  auto lock = GetImGuiLock();

  ImGui::GetIO().AddMousePosEvent(x, y);
}

void OnScreenUI::SetMousePress(u32 button_mask)
{
  auto lock = GetImGuiLock();

  for (size_t i = 0; i < std::size(ImGui::GetIO().MouseDown); i++)
  {
    ImGui::GetIO().AddMouseButtonEvent(static_cast<int>(i), (button_mask & (1u << i)) != 0);
  }
}

}  // namespace VideoCommon
