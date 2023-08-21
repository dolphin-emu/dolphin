// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"

class PointerWrap;

// This class is responsible for tracking the game's aspect ratio.
class WidescreenManager
{
public:
  WidescreenManager();

  bool IsGameWidescreen() const { return m_is_game_widescreen; }

  void DoState(PointerWrap& p);

private:
  void Update();
  void UpdateWidescreenHeuristic();

  bool m_is_game_widescreen = false;
  bool m_was_orthographically_anamorphic = false;

  Common::EventHook m_update_widescreen;
  Common::EventHook m_config_changed;
};

extern std::unique_ptr<WidescreenManager> g_widescreen;
