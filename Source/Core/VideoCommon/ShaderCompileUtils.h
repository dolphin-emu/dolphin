// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glslang/Public/ShaderLang.h>

namespace VideoCommon
{
class ShaderIncluder final : public glslang::TShader::Includer
{
public:
  ShaderIncluder(const std::string& user_path, const std::string& system_path);
  ~ShaderIncluder() override = default;

  std::vector<std::string> GetIncludes() const;

private:
  IncludeResult* includeLocal(const char* header_name, const char* includer_name,
                              std::size_t depth) override;
  IncludeResult* includeSystem(const char* header_name, const char* includer_name,
                               std::size_t depth) override;
  void releaseInclude(IncludeResult* result) override;

  IncludeResult* ProcessInclude(const std::string& root, const char* header_name,
                                const char* includer_name, std::size_t depth);
  std::string_view GetDirectory(std::string_view path) const;

  std::string m_root_user_path;
  std::string m_root_system_path;
  std::vector<std::string> m_dirs;

  struct IncludeResultData
  {
    std::unique_ptr<IncludeResult> result;
    std::string file_data;
  };
  std::map<std::string, IncludeResultData, std::less<>> m_include_results;
};

}  // namespace VideoCommon
