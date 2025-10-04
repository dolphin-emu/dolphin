// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/ShaderCompileUtils.h"

#include <fstream>
#include <ranges>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

namespace VideoCommon
{
ShaderIncluder::ShaderIncluder(const std::string& user_path, const std::string& system_path)
    : m_root_user_path(user_path), m_root_system_path(system_path)
{
}

std::vector<std::string> ShaderIncluder::GetIncludes() const
{
  const auto keys = std::views::keys(m_include_results);
  return {keys.begin(), keys.end()};
}

ShaderIncluder::IncludeResult*
ShaderIncluder::includeLocal(const char* header_name, const char* includer_name, std::size_t depth)
{
  return ProcessInclude(m_root_user_path, header_name, includer_name, depth);
}

ShaderIncluder::IncludeResult*
ShaderIncluder::includeSystem(const char* header_name, const char* includer_name, std::size_t depth)
{
  return ProcessInclude(m_root_system_path, header_name, includer_name, depth);
}

void ShaderIncluder::releaseInclude(IncludeResult* result)
{
  m_include_results.erase(result->headerName);
}

ShaderIncluder::IncludeResult* ShaderIncluder::ProcessInclude(const std::string& root,
                                                              const char* header_name,
                                                              const char* includer_name,
                                                              std::size_t depth)
{
  m_dirs.resize(depth);
  if (depth == 1)
  {
    const auto includer_dir = GetDirectory(includer_name);
    if (includer_dir == ".")
    {
      m_dirs.push_back(root);
    }
    else
    {
      m_dirs.push_back(std::string{includer_dir});
    }
  }

  // Search through directories back to front
  for (auto iter = m_dirs.rbegin(); iter != m_dirs.rend(); iter++)
  {
    const std::string full_path = WithUnifiedPathSeparators(*iter + '/' + header_name);

    std::string file_data;
    if (File::ReadFileToString(full_path, file_data))
    {
      m_dirs.push_back(std::string{GetDirectory(full_path)});

      const char* file_bytes = file_data.data();
      const std::size_t file_size = file_data.size();
      IncludeResultData result_data{
          .result = std::make_unique<IncludeResult>(full_path, file_bytes, file_size, nullptr),
          .file_data = std::move(file_data)};

      auto ptr = result_data.result.get();
      m_include_results[full_path] = std::move(result_data);
      return ptr;
    }
  }
  return nullptr;
}

std::string_view ShaderIncluder::GetDirectory(std::string_view path) const
{
  const auto last_pos = path.find_last_of('/');
  if (last_pos == std::string_view::npos)
    return ".";

  return path.substr(0, last_pos);
}
}  // namespace VideoCommon
