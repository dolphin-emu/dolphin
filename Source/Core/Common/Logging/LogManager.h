// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdarg>

#include "Common/BitSet.h"
#include "Common/Logging/Log.h"

namespace Common::Log
{
// pure virtual interface
class LogListener
{
public:
  virtual ~LogListener() = default;
  virtual void Log(LOG_LEVELS level, const char* msg) = 0;

  enum LISTENER
  {
    FILE_LISTENER = 0,
    CONSOLE_LISTENER,
    LOG_WINDOW_LISTENER,

    NUMBER_OF_LISTENERS  // Must be last
  };
};

class LogManager
{
public:
  static LogManager* GetInstance();
  static void Init();
  static void Shutdown();

  void Log(LOG_LEVELS level, LOG_TYPE type, const char* file, int line, const char* fmt,
           va_list args);
  void LogWithFullPath(LOG_LEVELS level, LOG_TYPE type, const char* file, int line, const char* fmt,
                       va_list args);

  LOG_LEVELS GetLogLevel() const;
  void SetLogLevel(LOG_LEVELS level);

  void SetEnable(LOG_TYPE type, bool enable);
  bool IsEnabled(LOG_TYPE type, LOG_LEVELS level = LNOTICE) const;

  const char* GetShortName(LOG_TYPE type) const;
  const char* GetFullName(LOG_TYPE type) const;

  void RegisterListener(LogListener::LISTENER id, LogListener* listener);
  void EnableListener(LogListener::LISTENER id, bool enable);
  bool IsListenerEnabled(LogListener::LISTENER id) const;

  void SaveSettings();

private:
  struct LogContainer
  {
    const char* m_short_name;
    const char* m_full_name;
    bool m_enable = false;
  };

  LogManager();
  ~LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;
  LogManager(LogManager&&) = delete;
  LogManager& operator=(LogManager&&) = delete;

  LOG_LEVELS m_level;
  std::array<LogContainer, NUMBER_OF_LOGS> m_log{};
  std::array<LogListener*, LogListener::NUMBER_OF_LISTENERS> m_listeners{};
  BitSet32 m_listener_ids;
  size_t m_path_cutoff_point = 0;
};
}  // namespace Common::Log
