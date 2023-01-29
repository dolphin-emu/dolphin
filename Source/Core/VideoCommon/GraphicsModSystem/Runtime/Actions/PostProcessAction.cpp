// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PostProcessAction.h"

#include <fmt/format.h>
#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/PEShaderSystem/Constants.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderGroup.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

std::unique_ptr<PostProcessAction> PostProcessAction::Create(const picojson::value& json_data,
                                                             std::string_view path)
{
  std::string profile_name;
  const auto& profile_name_json = json_data.get("profile_name");
  if (profile_name_json.is<std::string>())
  {
    profile_name = profile_name_json.get<std::string>();
  }
  if (profile_name.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Profile name not found for post processing action");
    return nullptr;
  }

  const std::string full_profile_path = fmt::format("{}/{}", path, profile_name);

  std::string profile_json_data;
  if (!File::ReadFileToString(full_profile_path, profile_json_data))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load profile '{}' for post processing action",
                  full_profile_path);
    return nullptr;
  }

  picojson::value root;
  const auto error = picojson::parse(root, profile_json_data);

  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load profile '{}' for post processing action due to parse error: {}",
                  full_profile_path, error);
    return nullptr;
  }

  if (!root.is<picojson::array>())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load profile '{}' for post processing action, root must contain an array!",
        full_profile_path);
    return nullptr;
  }

  auto serialized_shaders = root.get<picojson::array>();

  // Modify the shader path to be in this mod's directory
  for (auto& serialized_shader : serialized_shaders)
  {
    if (serialized_shader.is<picojson::object>())
    {
      auto& serialized_shader_obj = serialized_shader.get<picojson::object>();
      if (auto it = serialized_shader_obj.find("path"); it != serialized_shader_obj.end())
      {
        const auto new_path = fmt::format("{}/{}", path, it->second.get<std::string>());
        serialized_shader_obj.insert_or_assign("path", picojson::value(new_path));
      }
    }
  }

  VideoCommon::PE::ShaderConfigGroup config;
  config.DeserializeFromProfile(serialized_shaders);
  return std::make_unique<PostProcessAction>(std::move(config));
}

PostProcessAction::PostProcessAction(VideoCommon::PE::ShaderConfigGroup config)
    : m_config(std::move(config))
{
  m_timer.Start();
  m_pp_group = std::make_unique<VideoCommon::PE::ShaderGroup>();
  m_pp_group->UpdateConfig(m_config);

  const auto default_shader =
      fmt::format("{}/{}/{}", File::GetSysDirectory(),
                  VideoCommon::PE::Constants::dolphin_shipped_internal_shader_directory,
                  "DefaultVertexPixelShader.glsl");
  VideoCommon::PE::ShaderConfigGroup default_shader_config;
  default_shader_config.AddShader(default_shader, VideoCommon::PE::ShaderConfig::Source::System);

  m_default_shader_group = std::make_unique<VideoCommon::PE::ShaderGroup>();
  m_default_shader_group->UpdateConfig(default_shader_config);
}

PostProcessAction::~PostProcessAction()
{
  m_timer.Stop();
}

void PostProcessAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (m_pp_group->ShouldSkip())
    return;

  if (m_applied)
    return;

  m_applied = true;

  const u32 scissor_x_off = draw_started->scissors_x * 2;
  const u32 scissor_y_off = draw_started->scissors_y * 2;
  float x = g_renderer->EFBToScaledXf(draw_started->viewport_x - draw_started->viewport_width -
                                      scissor_x_off);
  float y = g_renderer->EFBToScaledYf(draw_started->viewport_y + draw_started->viewport_height -
                                      scissor_y_off);

  float width = g_renderer->EFBToScaledXf(2.0f * draw_started->viewport_width);
  float height = g_renderer->EFBToScaledYf(-2.0f * draw_started->viewport_height);
  if (width < 0.f)
  {
    x += width;
    width *= -1;
  }
  if (height < 0.f)
  {
    y += height;
    height *= -1;
  }

  // Lower-left flip.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = static_cast<float>(g_renderer->GetCurrentFramebuffer()->GetHeight()) - y - height;

  MathUtil::Rectangle<int> srcRect;
  srcRect.left = static_cast<int>(x);
  srcRect.top = static_cast<int>(y);
  srcRect.right = srcRect.left + static_cast<int>(width);
  srcRect.bottom = srcRect.top + static_cast<int>(height);

  const auto framebuffer_rect =
      g_renderer->ConvertFramebufferRectangle(srcRect, g_framebuffer_manager->GetEFBFramebuffer());

  VideoCommon::PE::ShaderApplyOptions options;
  options.m_camera_tan_half_fov_x = m_camera_tan_half_fov_x;
  options.m_camera_tan_half_fov_y = m_camera_tan_half_fov_y;
  options.m_dest_fb = g_framebuffer_manager->GetEFBFramebuffer();
  options.m_dest_rect = g_framebuffer_manager->GetEFBFramebuffer()->GetRect();
  options.m_source_color_tex = g_framebuffer_manager->ResolveEFBColorTexture(framebuffer_rect);
  options.m_source_depth_tex = g_framebuffer_manager->ResolveEFBDepthTexture(framebuffer_rect);
  options.m_source_layer = 0;
  options.m_source_rect = srcRect;
  options.m_time_elapsed = m_timer.ElapsedMs();

  m_pp_group->Apply(options, m_default_shader_group.get());
}

void PostProcessAction::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (m_pp_group->ShouldSkip())
    return;

  if (m_applied)
    return;

  m_applied = true;

  const auto scaled_src_rect = g_renderer->ConvertEFBRectangle(efb->src_rect);
  const auto framebuffer_rect = g_renderer->ConvertFramebufferRectangle(
      scaled_src_rect, g_framebuffer_manager->GetEFBFramebuffer());

  VideoCommon::PE::ShaderApplyOptions options;
  options.m_camera_tan_half_fov_x = m_camera_tan_half_fov_x;
  options.m_camera_tan_half_fov_y = m_camera_tan_half_fov_y;
  options.m_dest_fb = g_framebuffer_manager->GetEFBFramebuffer();
  options.m_dest_rect = g_framebuffer_manager->GetEFBFramebuffer()->GetRect();
  options.m_source_color_tex = g_framebuffer_manager->ResolveEFBColorTexture(framebuffer_rect);
  options.m_source_depth_tex = g_framebuffer_manager->ResolveEFBDepthTexture(framebuffer_rect);
  options.m_source_layer = 0;
  options.m_source_rect = efb->src_rect;
  options.m_time_elapsed = m_timer.ElapsedMs();

  m_pp_group->Apply(options, m_default_shader_group.get());
}

void PostProcessAction::OnProjection(GraphicsModActionData::Projection* projection)
{
  if (!projection->matrix) [[unlikely]]
    return;

  if (projection->projection_type == ProjectionType::Orthographic)
    return;

  m_camera_tan_half_fov_y = 1.0f / projection->matrix->data[5];
  m_camera_tan_half_fov_x = 1.0f / projection->matrix->data[0];
}

void PostProcessAction::OnProjectionAndTexture(GraphicsModActionData::Projection* projection)
{
  if (!projection->matrix) [[unlikely]]
    return;

  if (projection->projection_type == ProjectionType::Orthographic)
    return;

  m_camera_tan_half_fov_y = 1.0f / projection->matrix->data[5];
  m_camera_tan_half_fov_x = 1.0f / projection->matrix->data[0];
}

void PostProcessAction::OnFrameEnd()
{
  m_applied = false;
}
