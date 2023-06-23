// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "VideoCommon/GraphicsModSystem/Runtime/FBInfo.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/TextureInfo.h"
#include "VideoCommon/VideoEvents.h"
#include "VideoCommon/XFMemory.h"

class GraphicsModGroupConfig;
class GraphicsModManager
{
public:
  bool Initialize();

  const std::vector<GraphicsModAction*>& GetProjectionActions(ProjectionType projection_type) const;
  const std::vector<GraphicsModAction*>&
  GetProjectionTextureActions(ProjectionType projection_type,
                              const std::string& texture_name) const;
  const std::vector<GraphicsModAction*>&
  GetDrawStartedActions(const std::string& texture_name) const;
  const std::vector<GraphicsModAction*>&
  GetTextureLoadActions(const std::string& texture_name) const;
  const std::vector<GraphicsModAction*>&
  GetTextureCreateActions(const std::string& texture_name) const;
  const std::vector<GraphicsModAction*>& GetEFBActions(const FBInfo& efb) const;
  const std::vector<GraphicsModAction*>& GetXFBActions(const FBInfo& xfb) const;

  void Load(const GraphicsModGroupConfig& config);

private:
  void EndOfFrame();
  void Reset();

  class DecoratedAction;

  static inline const std::vector<GraphicsModAction*> m_default = {};
  std::list<std::unique_ptr<GraphicsModAction>> m_actions;
  std::unordered_map<ProjectionType, std::vector<GraphicsModAction*>>
      m_projection_target_to_actions;
  std::unordered_map<std::string, std::vector<GraphicsModAction*>>
      m_projection_texture_target_to_actions;
  std::unordered_map<std::string, std::vector<GraphicsModAction*>> m_draw_started_target_to_actions;
  std::unordered_map<std::string, std::vector<GraphicsModAction*>> m_load_texture_target_to_actions;
  std::unordered_map<std::string, std::vector<GraphicsModAction*>>
      m_create_texture_target_to_actions;
  std::unordered_map<FBInfo, std::vector<GraphicsModAction*>, FBInfoHasher> m_efb_target_to_actions;
  std::unordered_map<FBInfo, std::vector<GraphicsModAction*>, FBInfoHasher> m_xfb_target_to_actions;

  std::unordered_set<std::string> m_groups;

  Common::EventHook m_end_of_frame_event;
};

extern std::unique_ptr<GraphicsModManager> g_graphics_mod_manager;
