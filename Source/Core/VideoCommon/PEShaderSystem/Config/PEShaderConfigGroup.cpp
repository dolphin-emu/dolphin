// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigGroup.h"

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "VideoCommon/PEShaderSystem/Constants.h"

namespace VideoCommon::PE
{
bool ShaderConfigGroup::AddShader(const std::string& path, ShaderConfig::Source source)
{
  if (auto shader = ShaderConfig::Load(path, source))
  {
    m_shaders.push_back(*shader);
    const u32 index = static_cast<u32>(m_shaders.size());
    m_shader_order.push_back(index - 1);
    m_changes += 1;
    return true;
  }

  return false;
}

void ShaderConfigGroup::RemoveShader(u32 index)
{
  auto it = std::find_if(m_shader_order.begin(), m_shader_order.end(),
                         [index](u32 order_index) { return index == order_index; });
  if (it == m_shader_order.end())
    return;
  m_shader_order.erase(it);

  for (std::size_t i = 0; i < m_shader_order.size(); i++)
  {
    if (m_shader_order[i] > index)
    {
      m_shader_order[i]--;
    }
  }

  m_shaders.erase(m_shaders.begin() + index);
  m_changes += 1;
}

void ShaderConfigGroup::SerializeToProfile(picojson::value& value) const
{
  picojson::array serialized_shaders;
  for (const auto& shader : m_shaders)
  {
    picojson::object serialized_shader;
    shader.SerializeToProfile(serialized_shader);
    serialized_shaders.push_back(picojson::value{serialized_shader});
  }
  value = picojson::value{serialized_shaders};
}

void ShaderConfigGroup::DeserializeFromProfile(const picojson::array& serialized_shaders)
{
  if (serialized_shaders.empty())
  {
    return;
  }

  std::vector<ShaderConfig> new_group;
  for (const auto& serialized_shader : serialized_shaders)
  {
    if (serialized_shader.is<picojson::object>())
    {
      const auto serialized_shader_obj = serialized_shader.get<picojson::object>();

      std::string source;
      if (const auto it = serialized_shader_obj.find("source"); it != serialized_shader_obj.end())
      {
        source = it->second.to_str();
      }

      std::string shader_relative_path;
      if (const auto it = serialized_shader_obj.find("path"); it != serialized_shader_obj.end())
      {
        shader_relative_path = it->second.to_str();
      }

      if (source == "" || shader_relative_path == "")
      {
        continue;
      }

      if (source == "user")
      {
        const auto shader_full_path =
            fmt::format("{}{}", File::GetUserPath(D_SHADERS_IDX), shader_relative_path);
        if (const auto config = ShaderConfig::Load(shader_full_path, ShaderConfig::Source::User))
        {
          new_group.push_back(*config);
          new_group.back().DeserializeFromProfile(serialized_shader_obj);
        }
      }
      else if (source == "mod")
      {
        if (const auto config = ShaderConfig::Load(shader_relative_path, ShaderConfig::Source::Mod))
        {
          new_group.push_back(*config);
          new_group.back().DeserializeFromProfile(serialized_shader_obj);
        }
      }
      else if (source == "system")
      {
        const auto shader_full_path =
            fmt::format("{}{}/{}", File::GetSysDirectory(),
                        VideoCommon::PE::Constants::dolphin_shipped_public_shader_directory,
                        shader_relative_path);
        if (const auto config = ShaderConfig::Load(shader_full_path, ShaderConfig::Source::System))
        {
          new_group.push_back(*config);
          new_group.back().DeserializeFromProfile(serialized_shader_obj);
        }
      }
    }
  }

  if (!new_group.empty())
  {
    m_shaders = new_group;

    m_shader_order.clear();
    for (std::size_t i = 0; i < m_shaders.size(); i++)
    {
      m_shader_order.push_back(static_cast<u32>(i));
    }

    m_changes += 1;
  }
}

ShaderConfigGroup ShaderConfigGroup::CreateDefaultGroup()
{
  ShaderConfigGroup group;
  group.m_shaders.push_back(ShaderConfig::CreateDefaultShader());
  group.m_shader_order.push_back(0);
  group.m_changes += 1;
  return group;
}
}  // namespace VideoCommon::PE
