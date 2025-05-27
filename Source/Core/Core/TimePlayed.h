// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"
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

  void SetTimePlayed(const std::string& game_id, std::chrono::milliseconds time_played);

  std::chrono::milliseconds GetTimePlayed(const std::string& game_id) const;

  using UpdateEvent =
      Common::HookableEvent<"Time Played Update", const std::string&, std::chrono::milliseconds>;

  static constexpr int MAX_HOURS = 999'999;

private:
  TimePlayedManager();

  std::string m_ini_path;
  mutable std::mutex m_mutex;
  Common::IniFile m_ini;
  Common::IniFile::Section* m_time_list;
};
