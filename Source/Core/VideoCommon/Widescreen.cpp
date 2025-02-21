// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Widescreen.h"

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/System.h"

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"

std::unique_ptr<WidescreenManager> g_widescreen;

WidescreenManager::WidescreenManager()
{
  std::optional<bool> is_game_widescreen = GetWidescreenOverride();
  if (is_game_widescreen.has_value())
    m_is_game_widescreen = is_game_widescreen.value();

  // Throw a warning as unsupported aspect ratio modes have no specific behavior to them
  const bool is_valid_suggested_aspect_mode =
      g_ActiveConfig.suggested_aspect_mode == AspectMode::Auto ||
      g_ActiveConfig.suggested_aspect_mode == AspectMode::ForceStandard ||
      g_ActiveConfig.suggested_aspect_mode == AspectMode::ForceWide;
  if (!is_valid_suggested_aspect_mode)
  {
    WARN_LOG_FMT(VIDEO,
                 "Invalid suggested aspect ratio mode: only Auto, 4:3 and 16:9 are supported");
  }

  m_config_changed = ConfigChangedEvent::Register(
      [this](u32 bits) {
        if (bits & (CONFIG_CHANGE_BIT_ASPECT_RATIO))
        {
          std::optional<bool> is_game_widescreen = GetWidescreenOverride();
          // If the widescreen flag isn't being overridden by any settings,
          // reset it to default if heuristic aren't running or to the last
          // heuristic value if they were running.
          if (!is_game_widescreen.has_value())
            is_game_widescreen = (m_heuristic_state == HeuristicState::Active_Found_Anamorphic);
          if (is_game_widescreen.has_value())
            m_is_game_widescreen = is_game_widescreen.value();
        }
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

std::optional<bool> WidescreenManager::GetWidescreenOverride() const
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
  }

  return is_game_widescreen;
}

// Heuristic to detect if a GameCube game is in 16:9 anamorphic widescreen mode.
// Cheats that change the game aspect ratio to natively unsupported ones won't be recognized here.
void WidescreenManager::UpdateWidescreenHeuristic()
{
  // Reset to baseline state before the update
  const auto flush_statistics = g_vertex_manager->ResetFlushAspectRatioCount();
  const bool was_orthographically_anamorphic = m_was_orthographically_anamorphic;
  m_heuristic_state = HeuristicState::Inactive;
  m_was_orthographically_anamorphic = false;

  // If suggested_aspect_mode (GameINI) is configured don't use heuristic.
  // We also don't need to check "GetWidescreenOverride()" in this case as
  // nothing would have changed there.
  if (g_ActiveConfig.suggested_aspect_mode != AspectMode::Auto)
    return;

  std::optional<bool> is_game_widescreen = GetWidescreenOverride();

  // If widescreen hack isn't active and aspect_mode (UI) is 4:3 or 16:9 don't use heuristic.
  if (g_ActiveConfig.bWidescreenHack || (g_ActiveConfig.aspect_mode != AspectMode::ForceStandard &&
                                         g_ActiveConfig.aspect_mode != AspectMode::ForceWide))
  {
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

    g_stats.avg_persp_proj_viewport_ratio = persp.average_ratio.Mean();
    g_stats.avg_ortho_proj_viewport_ratio = ortho.average_ratio.Mean();

    const auto ortho_looks_anamorphic = looks_anamorphic(ortho);
    const auto persp_looks_normal = looks_normal(persp);

    if (looks_anamorphic(persp) || ortho_looks_anamorphic)
    {
      // If either perspective or orthographic projections look anamorphic, it's a safe bet.
      is_game_widescreen = true;
      m_heuristic_state = HeuristicState::Active_Found_Anamorphic;
    }
    else if (persp_looks_normal || looks_normal(ortho))
    {
      // Many widescreen games (or AR/GeckoCodes) use anamorphic perspective projections
      // with NON-anamorphic orthographic projections.
      // This can cause incorrect changes to 4:3 when perspective projections are temporarily not
      // shown. e.g. Animal Crossing's inventory menu.
      // Unless we were in a situation which was orthographically anamorphic
      // we won't consider orthographic data for changes from 16:9 to 4:3.
      if (persp_looks_normal || was_orthographically_anamorphic)
        is_game_widescreen = false;
      m_heuristic_state = HeuristicState::Active_Found_Normal;
    }
    else
    {
      m_heuristic_state = HeuristicState::Active_NotFound;
    }

    m_was_orthographically_anamorphic = ortho_looks_anamorphic;
  }

  if (is_game_widescreen.has_value())
    m_is_game_widescreen = is_game_widescreen.value();
}

void WidescreenManager::DoState(PointerWrap& p)
{
  p.Do(m_is_game_widescreen);

  if (p.IsReadMode())
  {
    m_was_orthographically_anamorphic = false;
    m_heuristic_state = HeuristicState::Inactive;
  }
}
