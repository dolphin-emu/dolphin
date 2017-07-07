// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdarg>
#include <string>

#include "Common/Logging/Log.h"
#include "Common/NonCopyable.h"

// pure virtual interface
class LogListener
{
public:
  virtual ~LogListener() {}
  virtual void Log(LogTypes::LOG_LEVELS, const char* msg) = 0;

  enum LISTENER
  {
    FILE_LISTENER = 0,
    CONSOLE_LISTENER,
    LOG_WINDOW_LISTENER,

    NUMBER_OF_LISTENERS  // Must be last
  };
};

class LogContainer;

class LogManager : NonCopyable
{
public:
  static LogManager* GetInstance();
  static void Init();
  static void Shutdown();

  void Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
           const char* fmt, va_list args);
  void LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
                       int line, const char* fmt, va_list args);

  LogTypes::LOG_LEVELS GetLogLevel() const;
  void SetLogLevel(LogTypes::LOG_LEVELS level);

  void SetEnable(LogTypes::LOG_TYPE type, bool enable);
  bool IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level = LogTypes::LNOTICE) const;

  std::string GetShortName(LogTypes::LOG_TYPE type) const;
  std::string GetFullName(LogTypes::LOG_TYPE type) const;
  void RegisterListener(LogListener::LISTENER id, LogListener* listener);
  void AddListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id);
  void RemoveListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id);

private:
  LogManager();
  ~LogManager();

  LogTypes::LOG_LEVELS m_level;
  LogContainer* m_log[LogTypes::NUMBER_OF_LOGS];
  std::array<LogListener*, LogListener::NUMBER_OF_LISTENERS> m_listeners{};
  size_t m_path_cutoff_point = 0;
};
