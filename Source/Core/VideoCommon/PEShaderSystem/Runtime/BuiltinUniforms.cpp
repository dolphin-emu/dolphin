// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/BuiltinUniforms.h"

#include "Core/Movie.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/RenderBase.h"

namespace VideoCommon::PE
{
BuiltinUniforms::BuiltinUniforms()
    : m_prev_resolution("prev_resolution"), m_prev_rect("prev_rect"),
      m_src_resolution("src_resolution"), m_window_resolution("window_resolution"),
      m_src_rect("src_rect"), m_time("u_time"), m_layer("u_layer"), m_output_scale("output_scale"),
      m_ndc_to_view_mul("ndc_to_view_mul"), m_ndc_to_view_add("ndc_to_view_add"),
      m_frames("frame_count")
{
}
std::size_t BuiltinUniforms::Size() const
{
  std::size_t result = 0;
  for (const auto* option : GetOptions())
  {
    result += option->Size();
  }
  return result;
}
void BuiltinUniforms::WriteToMemory(u8*& buffer) const
{
  for (const auto* option : GetOptions())
  {
    option->WriteToMemory(buffer);
  }
}
void BuiltinUniforms::WriteShaderUniforms(ShaderCode& shader_source) const
{
  for (const auto* option : GetOptions())
  {
    option->WriteShaderUniforms(shader_source);
  }
}

void BuiltinUniforms::Update(const ShaderApplyOptions& options,
                             const AbstractTexture* last_pass_texture, float last_pass_output_scale)
{
  const float prev_width_f = static_cast<float>(last_pass_texture->GetWidth());
  const float prev_height_f = static_cast<float>(last_pass_texture->GetHeight());
  const float rcp_prev_width = 1.0f / prev_width_f;
  const float rcp_prev_height = 1.0f / prev_height_f;
  m_prev_resolution.Update(
      std::array<float, 4>{prev_width_f, prev_height_f, rcp_prev_width, rcp_prev_height});

  m_prev_rect.Update(std::array<float, 4>{
      static_cast<float>(options.m_source_rect.left) * rcp_prev_width,
      static_cast<float>(options.m_source_rect.top) * rcp_prev_height,
      static_cast<float>(options.m_source_rect.GetWidth()) * rcp_prev_width,
      static_cast<float>(options.m_source_rect.GetHeight()) * rcp_prev_height});

  const float src_width_f = static_cast<float>(options.m_source_color_tex->GetWidth());
  const float src_height_f = static_cast<float>(options.m_source_color_tex->GetHeight());
  const float rcp_src_width = 1.0f / src_width_f;
  const float rcp_src_height = 1.0f / src_height_f;
  m_src_resolution.Update(
      std::array<float, 4>{src_width_f, src_height_f, rcp_src_width, rcp_src_height});

  const auto& window_rect = g_renderer->GetTargetRectangle();
  const float window_width_f = static_cast<float>(window_rect.GetWidth());
  const float window_height_f = static_cast<float>(window_rect.GetHeight());
  const float rcp_window_width = 1.0f / window_width_f;
  const float rcp_window_height = 1.0f / window_height_f;
  m_window_resolution.Update(
      std::array<float, 4>{window_width_f, window_height_f, rcp_window_width, rcp_window_height});

  const auto converted_src_rect = g_renderer->ConvertFramebufferRectangle(
      options.m_source_rect, options.m_source_color_tex->GetWidth(),
      options.m_source_color_tex->GetHeight());
  m_src_rect.Update(
      std::array<float, 4>{static_cast<float>(converted_src_rect.left) * rcp_src_width,
                           static_cast<float>(converted_src_rect.top) * rcp_src_height,
                           static_cast<float>(converted_src_rect.GetWidth()) * rcp_src_width,
                           static_cast<float>(converted_src_rect.GetHeight()) * rcp_src_height});

  m_time.Update(std::array<s32, 1>{static_cast<s32>(options.m_time_elapsed)});
  m_layer.Update(std::array<s32, 1>{static_cast<s32>(options.m_source_layer)});
  m_output_scale.Update(std::array<float, 1>{last_pass_output_scale});
  m_ndc_to_view_mul.Update(std::array<float, 2>{options.m_camera_tan_half_fov_x * 2.0f,
                                                options.m_camera_tan_half_fov_y * -2.0f});
  m_ndc_to_view_add.Update(std::array<float, 2>{options.m_camera_tan_half_fov_x * -1.0f,
                                                options.m_camera_tan_half_fov_y * 1.0f});
  m_frames.Update(std::array<s32, 1>{static_cast<s32>(Movie::GetCurrentFrame())});
}

std::array<const ShaderOption*, 11> BuiltinUniforms::GetOptions() const
{
  return {&m_prev_resolution, &m_prev_rect,
          &m_src_resolution,  &m_window_resolution,
          &m_src_rect,        &m_time,
          &m_layer,           &m_output_scale,
          &m_ndc_to_view_mul, &m_ndc_to_view_add,
          &m_frames};
}
}  // namespace VideoCommon::PE
