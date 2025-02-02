#pragma once

#include <string>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"
#include "TimePlayed.h"

// used for QT interface - general access to time played for games
TimePlayed::TimePlayed()
{
  m_game_id = "";
  m_ini_path = File::GetUserPath(D_CONFIG_IDX) + "TimePlayed.ini";
  m_ini.Load(m_ini_path);
  m_time_list = m_ini.GetOrCreateSection("Time Played");
}

TimePlayed::TimePlayed(std::string game_id)
{
  // filter for unsafe characters
  m_game_id = Common::EscapeFileName(game_id);
  m_ini_path = File::GetUserPath(D_CONFIG_IDX) + "TimePlayed.ini";
  m_ini.Load(m_ini_path);
  m_time_list = m_ini.GetOrCreateSection("Time Played");
}

void TimePlayed::AddTime(std::chrono::milliseconds time_emulated)
{
  if (m_game_id == "")
  {
    return;
  }

  u64 previous_time;
  m_time_list->Get(m_game_id, &previous_time);
  m_time_list->Set(m_game_id, previous_time + u64(time_emulated.count()));
  m_ini.Save(m_ini_path);
}

std::chrono::milliseconds TimePlayed::GetTimePlayed() const
{
  if (m_game_id == "")
  {
    return std::chrono::milliseconds(0);
  }

  u64 previous_time;
  m_time_list->Get(m_game_id, &previous_time);
  return std::chrono::milliseconds(previous_time);
}

std::chrono::milliseconds TimePlayed::GetTimePlayed(std::string game_id) const
{
  std::string filtered_game_id = Common::EscapeFileName(game_id);
  u64 previous_time;
  m_time_list->Get(filtered_game_id, &previous_time);
  return std::chrono::milliseconds(previous_time);
}

void TimePlayed::Reload()
{
  m_ini.Load(m_ini_path);
  m_time_list = m_ini.GetOrCreateSection("Time Played");
}
