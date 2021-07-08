// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOption.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigPass.h"

namespace VideoCommon::PE
{
// Note: this class can be accessed by both
// the video thread and the UI
// thread
class RuntimeInfo
{
public:
  bool HasError() const;
  void SetError(bool error);

private:
  std::atomic<bool> m_error;
};

struct ShaderConfig
{
  std::string m_name;
  std::string m_author;
  std::string m_description;
  std::string m_shader_path;

  enum class Source
  {
    Default,
    User,
    System
  };
  Source m_source;

  enum class Type
  {
    VertexPixel,
    Compute
  };
  Type m_type = Type::VertexPixel;

  std::vector<ShaderConfigOption> m_options;
  std::vector<ShaderConfigPass> m_passes;
  bool m_enabled = true;
  bool m_requires_depth_buffer = false;
  std::shared_ptr<RuntimeInfo> m_runtime_info = nullptr;

  u32 m_changes = 0;
  u32 m_compiletime_changes = 0;

  // bool IsValid() const;
  std::map<std::string, std::vector<ShaderConfigOption*>> GetGroups();
  void Reset();
  void Reload();
  void LoadSnapshot(std::size_t channel);
  void SaveSnapshot(std::size_t channel);
  bool HasSnapshot(std::size_t channel) const;

  bool DeserializeFromConfig(const picojson::value& value);

  void SerializeToProfile(picojson::object& value) const;
  void DeserializeFromProfile(const picojson::object& value);

  static std::optional<ShaderConfig> Load(const std::string& path, Source source);
  static ShaderConfig CreateDefaultShader();
};
}  // namespace VideoCommon::PE
