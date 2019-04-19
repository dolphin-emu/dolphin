// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <sstream>

#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/Movie.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/ImGuiManager.h"
#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderGen.h"

#include "imgui.h"

std::unique_ptr<VideoCommon::ImGuiManager> g_imgui_manager;

namespace VideoCommon
{
ImGuiManager::ImGuiManager(AbstractTextureFormat backbuffer_format, float backbuffer_scale)
    : m_backbuffer_format(backbuffer_format), m_backbuffer_scale(backbuffer_scale)
{
}

ImGuiManager::~ImGuiManager() = default;

static std::string GenerateImGuiVertexShader()
{
  const APIType api_type = g_ActiveConfig.backend_info.api_type;
  std::stringstream ss;

  // Uniform buffer contains the viewport size, and we transform in the vertex shader.
  if (api_type == APIType::D3D)
    ss << "cbuffer PSBlock : register(b0) {\n";
  else if (api_type == APIType::OpenGL)
    ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";
  else if (api_type == APIType::Vulkan)
    ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";
  ss << "float2 u_rcp_viewport_size_mul2;\n";
  ss << "};\n";

  if (api_type == APIType::D3D)
  {
    ss << "void main(in float2 rawpos : POSITION,\n"
       << "          in float2 rawtex0 : TEXCOORD,\n"
       << "          in float4 rawcolor0 : COLOR,\n"
       << "          out float2 frag_uv : TEXCOORD,\n"
       << "          out float4 frag_color : COLOR,\n"
       << "          out float4 out_pos : SV_Position)\n";
  }
  else
  {
    ss << "ATTRIBUTE_LOCATION(" << SHADER_POSITION_ATTRIB << ") in float2 rawpos;\n"
       << "ATTRIBUTE_LOCATION(" << SHADER_TEXTURE0_ATTRIB << ") in float2 rawtex0;\n"
       << "ATTRIBUTE_LOCATION(" << SHADER_COLOR0_ATTRIB << ") in float4 rawcolor0;\n"
       << "VARYING_LOCATION(0) out float2 frag_uv;\n"
       << "VARYING_LOCATION(1) out float4 frag_color;\n"
       << "void main()\n";
  }

  ss << "{\n"
     << "  frag_uv = rawtex0;\n"
     << "  frag_color = rawcolor0;\n";

  ss << "  " << (api_type == APIType::D3D ? "out_pos" : "gl_Position")
     << "= float4(rawpos.x * u_rcp_viewport_size_mul2.x - 1.0, 1.0 - rawpos.y * "
        "u_rcp_viewport_size_mul2.y, 0.0, 1.0);\n";

  // Clip-space is flipped in Vulkan
  if (api_type == APIType::Vulkan)
    ss << "  gl_Position.y = -gl_Position.y;\n";

  ss << "}\n";
  return ss.str();
}

static std::string GenerateImGuiPixelShader()
{
  const APIType api_type = g_ActiveConfig.backend_info.api_type;

  std::stringstream ss;
  if (api_type == APIType::D3D)
  {
    ss << "Texture2DArray tex0 : register(t0);\n"
       << "SamplerState samp0 : register(s0);\n"
       << "void main(in float2 frag_uv : TEXCOORD,\n"
       << "          in float4 frag_color : COLOR,\n"
       << "          out float4 ocol0 : SV_Target)\n";
  }
  else
  {
    ss << "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
       << "VARYING_LOCATION(0) in float2 frag_uv; \n"
       << "VARYING_LOCATION(1) in float4 frag_color;\n"
       << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n"
       << "void main()\n";
  }

  ss << "{\n";

  if (api_type == APIType::D3D)
    ss << "  ocol0 = tex0.Sample(samp0, float3(frag_uv, 0.0)) * frag_color;\n";
  else
    ss << "  ocol0 = texture(samp0, float3(frag_uv, 0.0)) * frag_color;\n";

  ss << "}\n";

  return ss.str();
}

bool ImGuiManager::Initialize()
{
  if (!ImGui::CreateContext())
  {
    PanicAlert("Creating ImGui context failed");
    return false;
  }

  // Don't create an ini file. TODO: Do we want this in the future?
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize.x = static_cast<float>(g_renderer->GetBackbufferWidth());
  io.DisplaySize.y = static_cast<float>(g_renderer->GetBackbufferHeight());
  io.DisplayFramebufferScale.x = m_backbuffer_scale;
  io.DisplayFramebufferScale.y = m_backbuffer_scale;
  io.FontGlobalScale = m_backbuffer_scale;
  ImGui::GetStyle().ScaleAllSizes(m_backbuffer_scale);

  PortableVertexDeclaration vdecl = {};
  vdecl.position = {VAR_FLOAT, 2, offsetof(ImDrawVert, pos), true, false};
  vdecl.texcoords[0] = {VAR_FLOAT, 2, offsetof(ImDrawVert, uv), true, false};
  vdecl.colors[0] = {VAR_UNSIGNED_BYTE, 4, offsetof(ImDrawVert, col), true, false};
  vdecl.stride = sizeof(ImDrawVert);
  m_imgui_vertex_format = g_renderer->CreateNativeVertexFormat(vdecl);
  if (!m_imgui_vertex_format)
  {
    PanicAlert("Failed to create imgui vertex format");
    return false;
  }

  const std::string vertex_shader_source = GenerateImGuiVertexShader();
  const std::string pixel_shader_source = GenerateImGuiPixelShader();
  std::unique_ptr<AbstractShader> vertex_shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Vertex, vertex_shader_source.c_str(), vertex_shader_source.size());
  std::unique_ptr<AbstractShader> pixel_shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Pixel, pixel_shader_source.c_str(), pixel_shader_source.size());
  if (!vertex_shader || !pixel_shader)
  {
    PanicAlert("Failed to compile imgui shaders");
    return false;
  }

  AbstractPipelineConfig pconfig = {};
  pconfig.vertex_format = m_imgui_vertex_format.get();
  pconfig.vertex_shader = vertex_shader.get();
  pconfig.pixel_shader = pixel_shader.get();
  pconfig.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  pconfig.depth_state = RenderState::GetNoDepthTestingDepthState();
  pconfig.blending_state = RenderState::GetNoBlendingBlendState();
  pconfig.blending_state.blendenable = true;
  pconfig.blending_state.srcfactor = BlendMode::SRCALPHA;
  pconfig.blending_state.dstfactor = BlendMode::INVSRCALPHA;
  pconfig.blending_state.srcfactoralpha = BlendMode::ZERO;
  pconfig.blending_state.dstfactoralpha = BlendMode::ONE;
  pconfig.framebuffer_state.color_texture_format = m_backbuffer_format;
  pconfig.framebuffer_state.depth_texture_format = AbstractTextureFormat::Undefined;
  pconfig.framebuffer_state.samples = 1;
  pconfig.framebuffer_state.per_sample_shading = false;
  pconfig.usage = AbstractPipelineUsage::Utility;
  m_imgui_pipeline = g_renderer->CreatePipeline(pconfig);
  if (!m_imgui_pipeline)
  {
    PanicAlert("Failed to create imgui pipeline");
    return false;
  }

  // Font texture(s).
  {
    u8* font_tex_pixels;
    int font_tex_width, font_tex_height;
    io.Fonts->GetTexDataAsRGBA32(&font_tex_pixels, &font_tex_width, &font_tex_height);

    TextureConfig font_tex_config(font_tex_width, font_tex_height, 1, 1, 1,
                                  AbstractTextureFormat::RGBA8, 0);
    std::unique_ptr<AbstractTexture> font_tex = g_renderer->CreateTexture(font_tex_config);
    if (!font_tex)
    {
      PanicAlert("Failed to create imgui texture");
      return false;
    }
    font_tex->Load(0, font_tex_width, font_tex_height, font_tex_width, font_tex_pixels,
                   sizeof(u32) * font_tex_width * font_tex_height);

    io.Fonts->TexID = font_tex.get();

    m_imgui_textures.push_back(std::move(font_tex));
  }

  m_last_frame_time = Common::Timer::GetTimeUs();
  ImGui::NewFrame();
  return true;
}

void ImGuiManager::Shutdown()
{
  ImGui::EndFrame();
  ImGui::DestroyContext();
  m_imgui_pipeline.reset();
  m_imgui_vertex_format.reset();
  m_imgui_textures.clear();
}

void ImGuiManager::Render()
{
  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (!draw_data || g_renderer->IsHeadless())
    return;

  ImGuiIO& io = ImGui::GetIO();
  g_renderer->SetViewport(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, 1.0f);

  // Uniform buffer for draws.
  struct ImGuiUbo
  {
    float u_rcp_viewport_size_mul2[2];
    float padding[2];
  };
  ImGuiUbo ubo = {{1.0f / io.DisplaySize.x * 2.0f, 1.0f / io.DisplaySize.y * 2.0f}};

  // Set up common state for drawing.
  g_renderer->SetPipeline(m_imgui_pipeline.get());
  g_renderer->SetSamplerState(0, RenderState::GetPointSamplerState());
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

      g_renderer->SetScissorRect(g_renderer->ConvertFramebufferRectangle(
          MathUtil::Rectangle<int>(
              static_cast<int>(cmd.ClipRect.x), static_cast<int>(cmd.ClipRect.y),
              static_cast<int>(cmd.ClipRect.z), static_cast<int>(cmd.ClipRect.w)),
          g_renderer->GetCurrentFramebuffer()));
      g_renderer->SetTexture(0, reinterpret_cast<const AbstractTexture*>(cmd.TextureId));
      g_renderer->DrawIndexed(base_index, cmd.ElemCount, base_vertex);
      base_index += cmd.ElemCount;
    }
  }
}

std::unique_lock<std::mutex> ImGuiManager::GetStateLock()
{
  return std::unique_lock<std::mutex>(m_state_mutex);
}

void ImGuiManager::EndFrame()
{
  std::unique_lock<std::mutex> imgui_lock(m_state_mutex);

  const u64 current_time_us = Common::Timer::GetTimeUs();
  const u64 time_diff_us = current_time_us - m_last_frame_time;
  const float time_diff_secs = static_cast<float>(time_diff_us / 1000000.0);
  m_last_frame_time = current_time_us;
  ImGui::NewFrame();

  // Update I/O with window dimensions.
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize.x = static_cast<float>(g_renderer->GetBackbufferWidth());
  io.DisplaySize.y = static_cast<float>(g_renderer->GetBackbufferHeight());
  io.DeltaTime = time_diff_secs;
}

void ImGuiManager::Draw()
{
  DrawFPSWindow();
  DrawMovieWindow();

  if (g_ActiveConfig.bOverlayStats)
    Statistics::Display();

  if (g_ActiveConfig.bShowNetPlayMessages && g_netplay_chat_ui)
    g_netplay_chat_ui->Display();

  if (Config::Get(Config::NETPLAY_GOLF_MODE_OVERLAY) && g_netplay_golf_ui)
    g_netplay_golf_ui->Display();

  if (g_ActiveConfig.bOverlayProjStats)
    Statistics::DisplayProj();

  OSD::DrawMessages();
}

void ImGuiManager::DrawFPSWindow()
{
  if (!g_ActiveConfig.bShowFPS)
    return;

  // Position in the top-right corner of the screen.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - (10.0f * m_backbuffer_scale),
                                 10.0f * m_backbuffer_scale),
                          ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(100.0f * m_backbuffer_scale, 30.0f * m_backbuffer_scale));

  if (ImGui::Begin("FPS", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "FPS: %.2f",
                       g_renderer->GetFPSCounter()->GetFPS());
  }
  ImGui::End();
}

void ImGuiManager::DrawMovieWindow()
{
  const auto& config = SConfig::GetInstance();
  const bool show_movie_window =
      config.m_ShowFrameCount | config.m_ShowLag | config.m_ShowInputDisplay | config.m_ShowRTC;
  if (!show_movie_window)
    return;

  // Position under the FPS display.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - (10.0f * m_backbuffer_scale),
                                 50.0f * m_backbuffer_scale),
                          ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(150.0f * m_backbuffer_scale, 20.0f * m_backbuffer_scale), ImGui::GetIO().DisplaySize);
  if (ImGui::Begin("Movie", nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    if (config.m_ShowFrameCount)
    {
      ImGui::Text("Frame: %" PRIu64, Movie::GetCurrentFrame());
    }
    if (Movie::IsPlayingInput())
    {
      ImGui::Text("Input: %" PRIu64 " / %" PRIu64, Movie::GetCurrentInputCount(),
                  Movie::GetTotalInputCount());
    }
    if (config.m_ShowLag)
      ImGui::Text("Lag: %" PRIu64 "\n", Movie::GetCurrentLagCount());
    if (config.m_ShowInputDisplay)
      ImGui::TextUnformatted(Movie::GetInputDisplay().c_str());
    if (config.m_ShowRTC)
      ImGui::TextUnformatted(Movie::GetRTCDisplay().c_str());
  }
  ImGui::End();
}

}  // namespace VideoCommon
