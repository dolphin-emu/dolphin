// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/PEShaderGroup.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEComputeShader.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShader.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon::PE
{
void ShaderGroup::Initialize()
{
}

void ShaderGroup::Shutdown()
{
}

void ShaderGroup::UpdateConfig(const ShaderConfigGroup& group)
{
  const bool needs_compile = group.m_changes != m_last_change_count;
  m_last_change_count = group.m_changes;
  if (needs_compile)
  {
    g_renderer->WaitForGPUIdle();
    if (!CreateShaders(group))
    {
      m_shaders.clear();
    }
  }
  else if (!m_shaders.empty())
  {
    m_skip = true;

    bool failure = false;
    std::size_t enabled_shader_index = 0;
    for (std::size_t i = 0; i < group.m_shader_order.size(); i++)
    {
      const u32& shader_index = group.m_shader_order[i];
      const auto& config_shader = group.m_shaders[shader_index];
      if (!config_shader.m_enabled)
        continue;

      m_skip = false;

      auto& shader = m_shaders[enabled_shader_index];
      if (!shader->UpdateConfig(config_shader))
      {
        failure = true;
        break;
      }

      enabled_shader_index++;
    }

    if (failure)
    {
      m_shaders.clear();
    }
  }
}

void ShaderGroup::Apply(const ShaderApplyOptions& options, ShaderGroup* skip_group)
{
  const u32 dest_rect_width = static_cast<u32>(options.m_dest_rect.GetWidth());
  const u32 dest_rect_height = static_cast<u32>(options.m_dest_rect.GetHeight());

  if (m_target_width != dest_rect_width || m_target_height != dest_rect_height ||
      m_target_layers != options.m_dest_fb->GetLayers() ||
      m_target_format != options.m_dest_fb->GetColorFormat())
  {
    const bool rebuild_pipelines = m_target_format != options.m_dest_fb->GetColorFormat() ||
                                   m_target_layers != options.m_dest_fb->GetLayers();
    m_target_width = dest_rect_width;
    m_target_height = dest_rect_height;
    m_target_layers = options.m_dest_fb->GetLayers();
    m_target_format = options.m_dest_fb->GetColorFormat();

    bool failure = false;
    const AbstractTexture* last_texture = options.m_source_color_tex;
    for (auto& shader : m_shaders)
    {
      if (!shader->CreateAllPassOutputTextures(dest_rect_width, dest_rect_height,
                                               options.m_dest_fb->GetLayers(),
                                               options.m_dest_fb->GetColorFormat()))
      {
        failure = true;
        break;
      }

      if (rebuild_pipelines)
      {
        if (!shader->RebuildPipeline(m_target_format, m_target_layers))
        {
          failure = true;
          break;
        }
      }
      const auto passes = shader->GetPasses();
      last_texture = passes.back()->m_output_texture.get();
    }

    if (failure)
    {
      // Clear shaders so we won't try again until rebuilt
      m_shaders.clear();
      return;
    }
  }

  const AbstractTexture* last_texture = options.m_source_color_tex;
  float output_scale = 1.0f;
  for (std::size_t i = 0; i < m_shaders.size() - 1; i++)
  {
    auto& shader = m_shaders[i];
    const bool skip_final_copy = false;
    shader->Apply(skip_final_copy, options, last_texture, output_scale);

    const auto passes = shader->GetPasses();
    const auto& last_pass = passes.back();
    last_texture = last_pass->m_output_texture.get();
    output_scale = last_pass->m_output_scale;
  }

  const auto& last_shader = m_shaders.back();
  const auto passes = last_shader->GetPasses();
  const auto& last_pass = passes.back();
  const bool last_pass_scaled = last_pass->m_output_scale != 1.0;
  const bool last_pass_uses_color_buffer =
      std::any_of(last_pass->m_inputs.begin(), last_pass->m_inputs.end(),
                  [](auto&& input) { return input->GetType() == InputType::ColorBuffer; });

  // Determine whether we can skip the final copy by writing directly to the output texture, if the
  // last pass is not scaled, and the target isn't multisampled.
  const bool skip_final_copy =
      !last_pass_scaled && !last_pass_uses_color_buffer &&
      options.m_dest_fb->GetColorAttachment() != options.m_source_color_tex &&
      options.m_dest_fb->GetSamples() == 1 && last_shader->SupportsDirectWrite();

  last_shader->Apply(skip_final_copy, options, last_texture, output_scale);

  if (!skip_final_copy && skip_group)
  {
    ShaderApplyOptions skip_options = options;
    skip_options.m_source_color_tex = last_pass->m_output_texture.get();
    skip_options.m_source_rect = last_pass->m_output_texture->GetRect();
    skip_group->Apply(skip_options);
  }
}

bool ShaderGroup::CreateShaders(const ShaderConfigGroup& group)
{
  m_target_width = 0;
  m_target_height = 0;
  m_target_format = AbstractTextureFormat::Undefined;
  m_target_layers = 0;

  m_shaders.clear();
  for (const u32& shader_index : group.m_shader_order)
  {
    const auto& config_shader = group.m_shaders[shader_index];
    if (!config_shader.m_enabled)
      continue;

    std::unique_ptr<BaseShader> shader;
    switch (config_shader.m_type)
    {
    case ShaderConfig::Type::VertexPixel:
      shader = std::make_unique<VertexPixelShader>();
      break;
    case ShaderConfig::Type::Compute:
    {
      if (!g_ActiveConfig.backend_info.bSupportsComputeShaders)
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to create compute shader, backend doesn't support compute shaders!");
        return false;
      }
      shader = std::make_unique<ComputeShader>();
      break;
    }
    }
    shader->CreateOptions(config_shader);
    if (!shader->CreatePasses(config_shader))
    {
      return false;
    }
    m_shaders.push_back(std::move(shader));
  }

  return true;
}
}  // namespace VideoCommon::PE
