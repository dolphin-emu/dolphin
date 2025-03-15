// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class TimePlayedManager
{
public:
  TimePlayedManager(const TimePlayedManager& other) = delete;
  TimePlayedManager(TimePlayedManager&& other) = delete;
  TimePlayedManager& operator=(const TimePlayedManager& other) = delete;
  TimePlayedManager& operator=(TimePlayedManager&& other) = delete;

  ~TimePlayedManager();

  static TimePlayedManager& GetInstance();

  void AddTime(const std::string& game_id, std::chrono::milliseconds time_emulated);

  std::chrono::milliseconds GetTimePlayed(const std::string& game_id) const;

  void Reload();

private:
  TimePlayedManager();

  std::string m_ini_path;
  mutable std::mutex m_mutex;
  Common::IniFile m_ini;
  Common::IniFile::Section* m_time_list;
};
