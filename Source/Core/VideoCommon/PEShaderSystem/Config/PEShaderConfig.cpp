// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"

#include <fmt/format.h>
#include <picojson.h>

#include <algorithm>
#include <set>
#include <sstream>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/PEShaderSystem/Constants.h"

namespace
{
std::string GetStreamAsString(std::ifstream& stream)
{
  std::stringstream ss;
  ss << stream.rdbuf();
  return ss.str();
}
}  // namespace

namespace VideoCommon::PE
{
bool RuntimeInfo::HasError() const
{
  return m_error.load();
}

void RuntimeInfo::SetError(bool error)
{
  m_error.store(error);
}

std::map<std::string, std::vector<ShaderConfigOption*>> ShaderConfig::GetGroups()
{
  std::map<std::string, std::vector<ShaderConfigOption*>> result;
  for (auto& option : m_options)
  {
    const std::string group = GetOptionGroup(option);
    result[group].push_back(&option);
  }
  return result;
}

void ShaderConfig::Reset()
{
  for (auto& option : m_options)
  {
    PE::Reset(option);
  }
  m_changes += 1;
}

void ShaderConfig::Reload()
{
  m_compiletime_changes += 1;
}

void ShaderConfig::LoadSnapshot(std::size_t channel)
{
  for (auto& option : m_options)
  {
    PE::LoadSnapshot(option, channel);
  }
  m_changes += 1;
}

void ShaderConfig::SaveSnapshot(std::size_t channel)
{
  for (auto& option : m_options)
  {
    PE::SaveSnapshot(option, channel);
  }
}

bool ShaderConfig::HasSnapshot(std::size_t channel) const
{
  return std::any_of(m_options.begin(), m_options.end(), [&](const ShaderConfigOption& option) {
    return PE::HasSnapshot(option, channel);
  });
}

bool ShaderConfig::DeserializeFromConfig(const picojson::value& value)
{
  const auto& meta = value.get("meta");
  if (meta.is<picojson::object>())
  {
    const auto& author = meta.get("author");
    if (author.is<std::string>())
    {
      m_author = author.to_str();
    }

    const auto& description = meta.get("description");
    if (description.is<std::string>())
    {
      m_description = description.to_str();
    }
  }

  const auto& type = value.get("type");
  if (type.is<std::string>())
  {
    auto type_str = type.to_str();
    std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (type_str == "vertex_pixel")
    {
      m_type = Type::VertexPixel;
    }
    else if (type_str == "compute")
    {
      m_type = Type::Compute;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, 'type' must be either "
                           "'vertex_pixel' or 'compute'");
      return false;
    }
  }
  else
  {
    INFO_LOG_FMT(VIDEO, "Missing 'type' or value is not a string.  Defaulting to 'vertex_pixel'");
    m_type = Type::VertexPixel;
  }

  const auto& options = value.get("options");
  if (options.is<picojson::array>())
  {
    for (const auto& option_val : options.get<picojson::array>())
    {
      if (!option_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load shader configuration file, specified option is not a json object");
        return false;
      }
      const auto option = DeserializeOptionFromConfig(option_val.get<picojson::object>());
      if (!option)
      {
        return false;
      }

      m_options.push_back(*option);
    }
  }

  std::set<std::string> known_outputs;
  const auto& passes = value.get("passes");
  if (passes.is<picojson::array>())
  {
    const auto& passes_arr = passes.get<picojson::array>();
    for (std::size_t i = 0; i < passes_arr.size(); i++)
    {
      const auto& pass_val = passes_arr[i];
      if (!pass_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load shader configuration file, specified pass is not a json object");
        return false;
      }
      ShaderConfigPass pass;
      if (!pass.DeserializeFromConfig(pass_val.get<picojson::object>(), i, known_outputs))
      {
        return false;
      }

      m_passes.push_back(std::move(pass));
    }
  }

  return true;
}

void ShaderConfig::SerializeToProfile(picojson::object& obj) const
{
  switch (m_source)
  {
  case Source::Default:
    return;
  case Source::Mod:
  {
    obj["source"] = picojson::value{"mod"};
    obj["path"] = picojson::value{m_shader_path};
  }
  break;
  case Source::User:
  {
    obj["source"] = picojson::value{"user"};

    const std::string base_path = File::GetUserPath(D_SHADERS_IDX);
    obj["path"] = picojson::value{m_shader_path.substr(base_path.size())};
  }
  break;
  case Source::System:
  {
    obj["source"] = picojson::value{"system"};
    const std::string base_path =
        fmt::format("{}{}/", File::GetSysDirectory(),
                    VideoCommon::PE::Constants::dolphin_shipped_public_shader_directory);
    obj["path"] = picojson::value{m_shader_path.substr(base_path.size())};
  }
  break;
  };

  picojson::array serialized_passes;
  for (const auto& pass : m_passes)
  {
    picojson::object serialized_pass;
    pass.SerializeToProfile(serialized_pass);
    serialized_passes.push_back(picojson::value{serialized_pass});
  }
  obj["passes"] = picojson::value{serialized_passes};

  picojson::array serialized_options;
  for (const auto& option : m_options)
  {
    picojson::object serialized_option;
    SerializeOptionToProfile(serialized_option, option);
    serialized_options.push_back(picojson::value{serialized_option});
  }
  obj["options"] = picojson::value{serialized_options};

  obj["enabled"] = picojson::value{m_enabled};
}

void ShaderConfig::DeserializeFromProfile(const picojson::object& value)
{
  if (const auto it = value.find("passes"); it != value.end())
  {
    if (it->second.is<picojson::array>())
    {
      auto serialized_passes = it->second.get<picojson::array>();
      if (serialized_passes.size() != m_passes.size())
        return;

      for (std::size_t i = 0; i < serialized_passes.size(); i++)
      {
        const auto& serialized_pass_val = serialized_passes[i];
        if (serialized_pass_val.is<picojson::object>())
        {
          const auto& serialized_pass = serialized_pass_val.get<picojson::object>();
          m_passes[i].DeserializeFromProfile(serialized_pass);
        }
      }
    }
  }

  if (const auto it = value.find("options"); it != value.end())
  {
    if (it->second.is<picojson::array>())
    {
      auto serialized_options = it->second.get<picojson::array>();
      if (serialized_options.size() != m_options.size())
        return;

      for (std::size_t i = 0; i < serialized_options.size(); i++)
      {
        const auto& serialized_option_val = serialized_options[i];
        if (serialized_option_val.is<picojson::object>())
        {
          const auto& serialized_option = serialized_option_val.get<picojson::object>();
          DeserializeOptionFromProfile(serialized_option, m_options[i]);
        }
      }
    }
  }

  if (const auto it = value.find("enabled"); it != value.end())
  {
    if (it->second.is<bool>())
    {
      m_enabled = it->second.get<bool>();
    }
  }
}

std::optional<ShaderConfig> ShaderConfig::Load(const std::string& shader_path, Source source)
{
  ShaderConfig result;
  result.m_runtime_info = std::make_shared<RuntimeInfo>();

  std::string basename;
  std::string base_path;
  SplitPath(shader_path, &base_path, &basename, nullptr);

  result.m_name = basename;
  result.m_shader_path = shader_path;
  result.m_source = source;

  const std::string config_file = fmt::format("{}/{}.json", base_path, basename);
  if (File::IsFile(config_file))
  {
    std::ifstream json_stream;
    File::OpenFStream(json_stream, config_file, std::ios_base::in);
    if (!json_stream.is_open())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file '{}'", config_file);
      return std::nullopt;
    }

    picojson::value root;
    const auto error = picojson::parse(root, GetStreamAsString(json_stream));

    if (!error.empty())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file '{}', due to parse error: {}",
                    config_file, error);
      return std::nullopt;
    }
    if (!result.DeserializeFromConfig(root))
    {
      return std::nullopt;
    }
  }
  else
  {
    result.m_description = "No description provided";
    result.m_author = "Unknown";
    result.m_type = Type::VertexPixel;
    result.m_passes.push_back(ShaderConfigPass::CreateDefaultPass());
  }

  return result;
}

ShaderConfig ShaderConfig::CreateDefaultShader()
{
  ShaderConfig result;
  result.m_name = "Default";
  result.m_description = "Default vertex/pixel shader";
  result.m_type = Type::VertexPixel;
  result.m_shader_path = fmt::format("{}/{}/{}", File::GetSysDirectory(),
                                     Constants::dolphin_shipped_internal_shader_directory,
                                     "DefaultVertexPixelShader.glsl");
  result.m_author = "Dolphin";
  result.m_source = Source::Default;
  result.m_passes.push_back(ShaderConfigPass::CreateDefaultPass());
  result.m_runtime_info = std::make_shared<RuntimeInfo>();
  return result;
}

}  // namespace VideoCommon::PE
