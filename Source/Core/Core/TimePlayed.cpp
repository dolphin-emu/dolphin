// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/TimePlayed.h"

#include <chrono>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/HookableEvent.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"

TimePlayedManager::TimePlayedManager()
    : m_ini_path(File::GetUserPath(D_CONFIG_IDX) + "TimePlayed.ini")
{
  m_ini.Load(m_ini_path);
  m_time_list = m_ini.GetOrCreateSection("TimePlayed");
}

TimePlayedManager::~TimePlayedManager() = default;

TimePlayedManager& TimePlayedManager::GetInstance()
{
  static TimePlayedManager time_played_manager;
  return time_played_manager;
}

void TimePlayedManager::AddTime(const std::string& game_id,
                                const std::chrono::milliseconds time_emulated)
{
  const std::string filtered_game_id = Common::EscapeFileName(game_id);
  u64 previous_time;
  u64 new_time;

  {
    std::lock_guard guard(m_mutex);

    m_time_list->Get(filtered_game_id, &previous_time);
    new_time = previous_time + static_cast<u64>(time_emulated.count());
    m_time_list->Set(filtered_game_id, new_time);
    m_ini.Save(m_ini_path);
  }

  UpdateEvent::Trigger(filtered_game_id, static_cast<std::chrono::milliseconds>(new_time));
}

std::chrono::milliseconds TimePlayedManager::GetTimePlayed(const std::string& game_id) const
{
  const std::string filtered_game_id = Common::EscapeFileName(game_id);
  u64 previous_time;

  std::lock_guard guard(m_mutex);
  m_time_list->Get(filtered_game_id, &previous_time);
  return std::chrono::milliseconds(previous_time);
}
