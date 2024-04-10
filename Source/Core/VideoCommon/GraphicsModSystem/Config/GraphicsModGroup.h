// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"

namespace GraphicsModSystem::Config
{
class GraphicsModGroup
{
public:
  explicit GraphicsModGroup(std::string game_id);

  void Load();
  void Save() const;

  void SetChangeCount(u32 change_count);
  u32 GetChangeCount() const;

  struct GraphicsModWithMetadata
  {
    GraphicsMod m_mod;
    std::string m_path;
    u64 m_id = 0;
    bool m_enabled = false;
    u16 m_weight = 0;
  };

  const std::vector<GraphicsModWithMetadata>& GetMods() const;
  std::vector<GraphicsModWithMetadata>& GetMods();

  GraphicsModWithMetadata* GetMod(u64 id) const;

  const std::string& GetGameID() const;

private:
  std::string GetPath() const;
  std::string m_game_id;
  std::vector<GraphicsModWithMetadata> m_graphics_mods;
  std::map<u64, GraphicsModWithMetadata*> m_id_to_graphics_mod;
  u32 m_change_count = 0;
};
}  // namespace GraphicsModSystem::Config
