// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/TimePlayed.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

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

void TimePlayed::TimeTrackerThreadFunc(std::stop_token stoken)
{
  Common::SetCurrentThreadName("Play Time Tracker");

  // Use a clock that will appropriately ignore suspended system time.
  Common::SteadyAwakeClock timer;
  auto prev_time = timer.now();

  static constexpr auto FILE_UPDATE_FREQ = std::chrono::seconds{30};

  std::mutex mutex;
  std::condition_variable_any cv;

  bool is_running = false;

  const auto core_state_changed = [&](Core::State state) {
    std::lock_guard lk{mutex};
    is_running = (state == Core::State::Running);
    cv.notify_one();
  };

  const auto hook = Core::AddOnStateChangedCallback(core_state_changed);
  core_state_changed(Core::GetState(Core::System::GetInstance()));

  std::unique_lock lk{mutex};
  while (true)
  {
    if (!is_running)
    {
      cv.wait(lk, stoken, [&] { return is_running; });
      // Once we're running again, adjust the reference time.
      prev_time = timer.now();
    }

    if (stoken.stop_requested())
      break;

    cv.wait_for(lk, stoken, FILE_UPDATE_FREQ, [&] { return !is_running; });

    const auto now = timer.now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev_time);
    prev_time = now;

    const std::string game_id = SConfig::GetInstance().GetGameID();

    INFO_LOG_FMT(CORE, "TimePlayed: Adding {} ms to {}", elapsed_ms.count(), game_id);
    TimePlayed{}.AddTime(game_id, elapsed_ms);
  }
}
