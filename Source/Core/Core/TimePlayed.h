#pragma once
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class TimePlayed
{
public:
  TimePlayed();
  TimePlayed(std::string game_id);

  void AddTime(std::chrono::milliseconds time_emulated);

  std::chrono::milliseconds GetTimePlayed() const;
  std::chrono::milliseconds GetTimePlayed(std::string game_id) const;

  void Reload();

private:
  std::string m_game_id;
  Common::IniFile m_ini;
  std::string m_ini_path;

  Common::IniFile::Section* m_time_list;
};
