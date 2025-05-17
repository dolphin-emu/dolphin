// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/TimePlayed.h"

#include <chrono>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"

TimePlayed::TimePlayed() : m_ini_path(File::GetUserPath(D_CONFIG_IDX) + "TimePlayed.ini")
{
  Reload();
}

TimePlayed::~TimePlayed() = default;

void TimePlayed::AddTime(const std::string& game_id, std::chrono::milliseconds time_emulated)
{
  std::string filtered_game_id = Common::EscapeFileName(game_id);
  u64 previous_time;
  m_time_list->Get(filtered_game_id, &previous_time);
  m_time_list->Set(filtered_game_id, previous_time + static_cast<u64>(time_emulated.count()));
  m_ini.Save(m_ini_path);
}

std::chrono::milliseconds TimePlayed::GetTimePlayed(const std::string& game_id) const
{
  std::string filtered_game_id = Common::EscapeFileName(game_id);
  u64 previous_time;
  m_time_list->Get(filtered_game_id, &previous_time);
  return std::chrono::milliseconds(previous_time);
}

void TimePlayed::Reload()
{
  m_ini.Load(m_ini_path);
  m_time_list = m_ini.GetOrCreateSection("TimePlayed");
}
