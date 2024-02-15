// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

struct GraphicsModConfig;

class GraphicsModGroupConfig
{
public:
  explicit GraphicsModGroupConfig(std::string game_id);
  ~GraphicsModGroupConfig();

  GraphicsModGroupConfig(const GraphicsModGroupConfig&);
  GraphicsModGroupConfig(GraphicsModGroupConfig&&) noexcept;

  GraphicsModGroupConfig& operator=(const GraphicsModGroupConfig&);
  GraphicsModGroupConfig& operator=(GraphicsModGroupConfig&&) noexcept;

  void Load();
  void Save() const;

  void SetChangeCount(u32 change_count);
  u32 GetChangeCount() const;

  const std::vector<GraphicsModConfig>& GetMods() const;
  std::vector<GraphicsModConfig>& GetMods();

  GraphicsModConfig* GetMod(std::string_view absolute_path) const;

  const std::string& GetGameID() const;

private:
  std::string GetPath() const;
  std::string m_game_id;
  std::vector<GraphicsModConfig> m_graphics_mods;
  std::map<std::string, GraphicsModConfig*, std::less<>> m_path_to_graphics_mod;
  u32 m_change_count = 0;
};
