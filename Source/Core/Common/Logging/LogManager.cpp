// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <locale>
#include <mutex>
#include <ostream>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

const std::array<LogTypes::Info, LogTypes::NUMBER_OF_LOGS> LogTypes::INFO{{
    {"All", "All Logs", LogTypes::NUMBER_OF_LOGS},
#define TYPE(id, short_name, long_name) {short_name, long_name},
#define CATEGORY(id, short_name, long_name) {short_name, long_name, id##_END},
#define END_CATEGORY(id) {},
#include "Common/Logging/LogTypes.h"
#undef TYPE
#undef CATEGORY
#undef END_CATEGORY
}};

bool LogTypes::IsLeaf(LOG_TYPE type)
{
  ASSERT(IsValid(type));
  return INFO[type].end <= type;
}

bool LogTypes::IsValid(LOG_TYPE type)
{
  return type >= ALL_LOGS && type < NUMBER_OF_LOGS && INFO[type].short_name;
}

LogTypes::LOG_TYPE LogTypes::FirstChild(LOG_TYPE type)
{
  ASSERT(!IsLeaf(type));
  return LOG_TYPE(size_t(type) + 1);
}

LogTypes::LOG_TYPE LogTypes::NextSibling(LOG_TYPE type)
{
  ASSERT(IsValid(type));
  if (INFO[type].end > type)
  {
    return LOG_TYPE(INFO[type].end + 1);
  }
  return LOG_TYPE(type + 1);
}

LogTypes::LOG_TYPE LogTypes::Parent(LOG_TYPE type)
{
  ASSERT(IsValid(type));
  if (type == ALL_LOGS)
  {
    return NUMBER_OF_LOGS;
  }
  for (size_t i = size_t(type) - 1; i + 1 != 0; --i)
  {
    if (INFO[i].end >= type)
    {
      return LOG_TYPE(i);
    }
  }
  return NUMBER_OF_LOGS;
}

size_t LogTypes::CountChildren(LOG_TYPE type)
{
  if (IsLeaf(type))
  {
    return 0;
  }
  size_t count = 0;
  for (LOG_TYPE t = FirstChild(type); IsValid(t); t = NextSibling(t))
  {
    ++count;
  }
  return count;
}

constexpr size_t MAX_MSGLEN = 1024;

const Config::ConfigInfo<bool> LOGGER_WRITE_TO_FILE{
    {Config::System::Logger, "Options", "WriteToFile"}, false};
const Config::ConfigInfo<bool> LOGGER_WRITE_TO_CONSOLE{
    {Config::System::Logger, "Options", "WriteToConsole"}, true};
const Config::ConfigInfo<bool> LOGGER_WRITE_TO_WINDOW{
    {Config::System::Logger, "Options", "WriteToWindow"}, true};
const Config::ConfigInfo<u8> LOGGER_VERBOSITY{{Config::System::Logger, "Options", "Verbosity"},
                                              LogTypes::LERROR};

class FileLogListener : public LogListener
{
public:
  FileLogListener(const std::string& filename)
  {
    File::OpenFStream(m_logfile, filename, std::ios::app);
    SetEnable(true);
  }

  void Log(LogTypes::LOG_LEVELS, const char* msg) override
  {
    if (!IsEnabled() || !IsValid())
      return;

    std::lock_guard<std::mutex> lk(m_log_lock);
    m_logfile << msg << std::flush;
  }

  bool IsValid() const { return m_logfile.good(); }
  bool IsEnabled() const { return m_enable; }
  void SetEnable(bool enable) { m_enable = enable; }

private:
  std::mutex m_log_lock;
  std::ofstream m_logfile;
  bool m_enable;
};

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
                const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  if (LogManager::GetInstance())
    LogManager::GetInstance()->Log(level, type, file, line, fmt, args);
  va_end(args);
}

static size_t DeterminePathCutOffPoint()
{
  constexpr const char* pattern = "/source/core/";
#ifdef _WIN32
  constexpr const char* pattern2 = "\\source\\core\\";
#endif
  std::string path = __FILE__;
  std::transform(path.begin(), path.end(), path.begin(),
                 [](char c) { return std::tolower(c, std::locale::classic()); });
  size_t pos = path.find(pattern);
#ifdef _WIN32
  if (pos == std::string::npos)
    pos = path.find(pattern2);
#endif
  if (pos != std::string::npos)
    return pos + strlen(pattern);
  return 0;
}

LogManager::LogManager()
{
  RegisterListener(LogListener::FILE_LISTENER,
                   new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
  RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

  // Set up log listeners
  EnableListener(LogListener::FILE_LISTENER, Config::Get(LOGGER_WRITE_TO_FILE));
  EnableListener(LogListener::CONSOLE_LISTENER, Config::Get(LOGGER_WRITE_TO_CONSOLE));
  EnableListener(LogListener::LOG_WINDOW_LISTENER, Config::Get(LOGGER_WRITE_TO_WINDOW));

  // pre-initialize root with legacy Verbosity key
  u8 verbosity = Config::Get(LOGGER_VERBOSITY);
  if (verbosity < 1)
    verbosity = 1;
  if (verbosity > MAX_LOGLEVEL)
    verbosity = MAX_LOGLEVEL;
  m_log[LogTypes::ALL_LOGS].log_level = LogTypes::LOG_LEVELS(verbosity);

  for (size_t pos = 0; pos < LogTypes::NUMBER_OF_LOGS; ++pos)
  {
    if (!LogTypes::IsValid(LogTypes::LOG_TYPE(pos)))
    {
      continue;
    }
    auto& info = LogTypes::INFO[pos];
    const char* sname = info.short_name;
    const Config::ConfigInfo<std::string> conf{{Config::System::Logger, "Logs", sname}, ""};
    auto str = Config::Get(conf);
    u8 log_level = 0;
    bool enable = false;
    if (TryParse(str, &log_level))
    {
      if (log_level > MAX_LOGLEVEL && log_level != LogTypes::LINHERIT)
      {
        log_level = MAX_LOGLEVEL;
      }
      if (log_level != LogTypes::LINHERIT)
      {
        m_log[pos].log_level = LogTypes::LOG_LEVELS(log_level);
        m_log[pos].overridden = true;
      }
    }
    else if (TryParse(str, &enable))
    {
      // legacy format (boolean)
      m_log[pos].log_level = LogTypes::LOG_LEVELS(enable ? verbosity : 0);
      m_log[pos].overridden = m_log[LogTypes::ALL_LOGS].log_level != m_log[pos].log_level;
    }
    // propagate into children
    for (size_t i = pos + 1; i < info.end; ++i)
    {
      m_log[i].log_level = m_log[pos].log_level;
    }
  }

  m_path_cutoff_point = DeterminePathCutOffPoint();
}

LogManager::~LogManager()
{
  // The log window listener pointer is owned by the GUI code.
  delete m_listeners[LogListener::CONSOLE_LISTENER];
  delete m_listeners[LogListener::FILE_LISTENER];
}

void LogManager::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_FILE, IsListenerEnabled(LogListener::FILE_LISTENER));
  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_CONSOLE,
                           IsListenerEnabled(LogListener::CONSOLE_LISTENER));
  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_WINDOW,
                           IsListenerEnabled(LogListener::LOG_WINDOW_LISTENER));

  const Config::ConfigInfo<u8> root{{Config::System::Logger, "Logs", "All"},
                                    LOGGER_VERBOSITY.default_value};
  Config::SetBaseOrCurrent(root, u8(m_log[LogTypes::ALL_LOGS].log_level));
  for (size_t pos = LogTypes::ALL_LOGS + 1; pos < LogTypes::NUMBER_OF_LOGS; ++pos)
  {
    if (!LogTypes::IsValid(LogTypes::LOG_TYPE(pos)))
    {
      continue;
    }
    const char* name = LogTypes::INFO[pos].short_name;
    const Config::ConfigInfo<u8> info{{Config::System::Logger, "Logs", name}, LogTypes::LINHERIT};
    Config::SetBaseOrCurrent(info, m_log[pos].overridden ? u8(m_log[pos].log_level) :
                                                           u8(LogTypes::LINHERIT));
  }

  Config::Save();
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
                     int line, const char* format, va_list args)
{
  return LogWithFullPath(level, type, file + m_path_cutoff_point, line, format, args);
}

void LogManager::LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
                                 const char* file, int line, const char* format, va_list args)
{
  if (!IsEnabled(type, level) || !static_cast<bool>(m_listener_ids))
    return;

  char temp[MAX_MSGLEN];
  CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

  std::string msg = StringFromFormat(
      "%s %s:%u %c[%s]: %s\n", Common::Timer::GetTimeFormatted().c_str(), file, line,
      LogTypes::LOG_LEVEL_TO_CHAR[(int)level], LogTypes::INFO[type].short_name, temp);

  for (auto listener_id : m_listener_ids)
    if (m_listeners[listener_id])
      m_listeners[listener_id]->Log(level, msg.c_str());
}

LogTypes::LOG_LEVELS LogManager::GetLogLevel(LogTypes::LOG_TYPE type) const
{
  return m_log[type].log_level;
}

void LogManager::SetLogLevel(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level)
{
  ASSERT(LogTypes::IsValid(type));
  if (level != LogTypes::LINHERIT)
  {
    m_log[type].log_level = level;
  }
  else if (type != 0)
  {
    auto p = LogTypes::Parent(type);
    ASSERT(LogTypes::IsValid(p));
    m_log[type].log_level = m_log[p].log_level;
  }

  // propagate into descendents
  for (size_t i = size_t(type) + 1; i < LogTypes::INFO[type].end; ++i)
  {
    if (!m_log[i].overridden)
    {
      m_log[i].log_level = level;
    }
    else if (LogTypes::INFO[i].end > i)
    {
      // skip children of overridden children
      i = LogTypes::INFO[i].end;
    }
  }
}

bool LogManager::IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level) const
{
  return m_log[type].log_level >= level;
}

void LogManager::RegisterListener(LogListener::LISTENER id, LogListener* listener)
{
  m_listeners[id] = listener;
}

void LogManager::EnableListener(LogListener::LISTENER id, bool enable)
{
  m_listener_ids[id] = enable;
}

bool LogManager::IsListenerEnabled(LogListener::LISTENER id) const
{
  return m_listener_ids[id];
}

// Singleton. Ugh.
static LogManager* s_log_manager;

LogManager* LogManager::GetInstance()
{
  return s_log_manager;
}

void LogManager::Init()
{
  s_log_manager = new LogManager();
}

void LogManager::Shutdown()
{
  if (s_log_manager)
    s_log_manager->SaveSettings();
  delete s_log_manager;
  s_log_manager = nullptr;
}
