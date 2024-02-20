// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Widescreen.h"

#include "Common/ChunkFile.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/System.h"

#include "VideoCommon/VertexManagerBase.h"

std::unique_ptr<WidescreenManager> g_widescreen;

WidescreenManager::WidescreenManager()
{
  Update();

  m_config_changed = ConfigChangedEvent::Register(
      [this](u32 bits) {
        if (bits & (CONFIG_CHANGE_BIT_ASPECT_RATIO))
          Update();
      },
      "Widescreen");

  // VertexManager doesn't maintain statistics in Wii mode.
  auto& system = Core::System::GetInstance();
  if (!system.IsWii())
  {
    m_update_widescreen = AfterFrameEvent::Register(
        [this](Core::System&) { UpdateWidescreenHeuristic(); }, "WideScreen Heuristic");
  }
}

void WidescreenManager::Update()
{
  std::optional<bool> is_game_widescreen;

  auto& system = Core::System::GetInstance();
  if (system.IsWii())
    is_game_widescreen = Config::Get(Config::SYSCONF_WIDESCREEN);

  // suggested_aspect_mode overrides SYSCONF_WIDESCREEN
  if (g_ActiveConfig.suggested_aspect_mode == AspectMode::ForceStandard)
    is_game_widescreen = false;
  else if (g_ActiveConfig.suggested_aspect_mode == AspectMode::ForceWide)
    is_game_widescreen = true;

  // If widescreen hack is disabled override game's AR if UI is set to 4:3 or 16:9.
  if (!g_ActiveConfig.bWidescreenHack)
  {
    const auto aspect_mode = g_ActiveConfig.aspect_mode;
    if (aspect_mode == AspectMode::ForceStandard)
      is_game_widescreen = false;
    else if (aspect_mode == AspectMode::ForceWide)
      is_game_widescreen = true;

    // Reset settings to default if heuristics aren't currently running and
    // the user selected the automatic aspect ratio.
    if (!is_game_widescreen.has_value() && !m_widescreen_heuristics_active_and_successful &&
        aspect_mode == AspectMode::Auto)
    {
      is_game_widescreen = false;
    }
  }

  if (is_game_widescreen.has_value())
  {
    m_is_game_widescreen = is_game_widescreen.value();
  }
}

// Heuristic to detect if a GameCube game is in 16:9 anamorphic widescreen mode.
// Cheats that change the game aspect ratio to natively unsupported ones won't be recognized here.
void WidescreenManager::UpdateWidescreenHeuristic()
{
  const auto flush_statistics = g_vertex_manager->ResetFlushAspectRatioCount();

  // If suggested_aspect_mode (GameINI) is configured don't use heuristic.
  if (g_ActiveConfig.suggested_aspect_mode != AspectMode::Auto)
  {
    m_widescreen_heuristics_active_and_successful = false;
    return;
  }

  Update();

  m_widescreen_heuristics_active_and_successful = false;

  // If widescreen hack isn't active and aspect_mode (UI) is 4:3 or 16:9 don't use heuristic.
  if (!g_ActiveConfig.bWidescreenHack && (g_ActiveConfig.aspect_mode == AspectMode::ForceStandard ||
                                          g_ActiveConfig.aspect_mode == AspectMode::ForceWide))
    return;

  // Modify the threshold based on which aspect ratio we're already using:
  // If the game's in 4:3, it probably won't switch to anamorphic, and vice-versa.
  const u32 transition_threshold = g_ActiveConfig.widescreen_heuristic_transition_threshold;

  const auto looks_normal = [transition_threshold](auto& counts) {
    return counts.normal_vertex_count > counts.anamorphic_vertex_count * transition_threshold;
  };
  const auto looks_anamorphic = [transition_threshold](auto& counts) {
    return counts.anamorphic_vertex_count > counts.normal_vertex_count * transition_threshold;
  };

  const auto& persp = flush_statistics.perspective;
  const auto& ortho = flush_statistics.orthographic;

  const auto ortho_looks_anamorphic = looks_anamorphic(ortho);

  if (looks_anamorphic(persp) || ortho_looks_anamorphic)
  {
    // If either perspective or orthographic projections look anamorphic, it's a safe bet.
    m_is_game_widescreen = true;
    m_widescreen_heuristics_active_and_successful = true;
  }
  else if (looks_normal(persp) || (m_was_orthographically_anamorphic && looks_normal(ortho)))
  {
    // Many widescreen games (or AR/GeckoCodes) use anamorphic perspective projections
    // with NON-anamorphic orthographic projections.
    // This can cause incorrect changes to 4:3 when perspective projections are temporarily not
    // shown. e.g. Animal Crossing's inventory menu.
    // Unless we were in a situation which was orthographically anamorphic
    // we won't consider orthographic data for changes from 16:9 to 4:3.
    m_is_game_widescreen = false;
    m_widescreen_heuristics_active_and_successful = true;
  }

  m_was_orthographically_anamorphic = ortho_looks_anamorphic;
}

void WidescreenManager::DoState(PointerWrap& p)
{
  p.Do(m_is_game_widescreen);

  if (p.IsReadMode())
  {
    m_was_orthographically_anamorphic = false;
    m_widescreen_heuristics_active_and_successful = false;
  }
}
