// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdarg>
#include <map>
#include <string>

#include "Common/BitSet.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"

namespace Common::Log
{
// pure virtual interface
class LogListener
{
public:
  virtual ~LogListener() = default;
  virtual void Log(LogLevel level, const char* msg) = 0;

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

  void Log(LogLevel level, LogType type, const char* file, int line, const char* message);
  void LogWithFullPath(LogLevel level, LogType type, const char* file, int line,
                       const char* message);

  LogLevel GetLogLevel() const;
  void SetLogLevel(LogLevel level);

  void SetEnable(LogType type, bool enable);
  bool IsEnabled(LogType type, LogLevel level = LogLevel::LNOTICE) const;

  void SetEnableConvertSJIS(bool enable) { m_convert_sjis = enable; }
  bool IsEnabledConvertSJIS() const { return m_convert_sjis; }

  std::map<std::string, std::string> GetLogTypes();

  const char* GetShortName(LogType type) const;
  const char* GetFullName(LogType type) const;

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

  bool m_convert_sjis = true;

  LogManager();
  ~LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;
  LogManager(LogManager&&) = delete;
  LogManager& operator=(LogManager&&) = delete;

  static std::string GetTimestamp();

  LogLevel m_level;
  EnumMap<LogContainer, LAST_LOG_TYPE> m_log{};
  std::array<LogListener*, LogListener::NUMBER_OF_LISTENERS> m_listeners{};
  BitSet32 m_listener_ids;
  size_t m_path_cutoff_point = 0;
};
}  // namespace Common::Log
