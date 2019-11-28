// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/LogManager.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <locale>
#include <mutex>
#include <ostream>
#include <string>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

namespace Common::Log
{
constexpr size_t MAX_MSGLEN = 1024;

const Config::ConfigInfo<bool> LOGGER_WRITE_TO_FILE{
    {Config::System::Logger, "Options", "WriteToFile"}, false};
const Config::ConfigInfo<bool> LOGGER_WRITE_TO_CONSOLE{
    {Config::System::Logger, "Options", "WriteToConsole"}, true};
const Config::ConfigInfo<bool> LOGGER_WRITE_TO_WINDOW{
    {Config::System::Logger, "Options", "WriteToWindow"}, true};
const Config::ConfigInfo<int> LOGGER_VERBOSITY{{Config::System::Logger, "Options", "Verbosity"}, 0};

class FileLogListener : public LogListener
{
public:
  FileLogListener(const std::string& filename)
  {
    File::OpenFStream(m_logfile, filename, std::ios::app);
    SetEnable(true);
  }

  void Log(LOG_LEVELS, const char* msg) override
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

void GenericLog(LOG_LEVELS level, LOG_TYPE type, const char* file, int line, const char* fmt, ...)
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
  // create log containers
  m_log[ACTIONREPLAY] = {"ActionReplay", "ActionReplay"};
  m_log[AUDIO] = {"Audio", "Audio Emulator"};
  m_log[AUDIO_INTERFACE] = {"AI", "Audio Interface"};
  m_log[BOOT] = {"BOOT", "Boot"};
  m_log[COMMANDPROCESSOR] = {"CP", "CommandProc"};
  m_log[COMMON] = {"COMMON", "Common"};
  m_log[CONSOLE] = {"CONSOLE", "Dolphin Console"};
  m_log[CORE] = {"CORE", "Core"};
  m_log[DISCIO] = {"DIO", "Disc IO"};
  m_log[DSPHLE] = {"DSPHLE", "DSP HLE"};
  m_log[DSPLLE] = {"DSPLLE", "DSP LLE"};
  m_log[DSP_MAIL] = {"DSPMails", "DSP Mails"};
  m_log[DSPINTERFACE] = {"DSP", "DSPInterface"};
  m_log[DVDINTERFACE] = {"DVD", "DVD Interface"};
  m_log[DYNA_REC] = {"JIT", "Dynamic Recompiler"};
  m_log[EXPANSIONINTERFACE] = {"EXI", "Expansion Interface"};
  m_log[FILEMON] = {"FileMon", "File Monitor"};
  m_log[GDB_STUB] = {"GDB_STUB", "GDB Stub"};
  m_log[GPFIFO] = {"GP", "GPFifo"};
  m_log[HOST_GPU] = {"Host GPU", "Host GPU"};
  m_log[IOS] = {"IOS", "IOS"};
  m_log[IOS_DI] = {"IOS_DI", "IOS - Drive Interface"};
  m_log[IOS_ES] = {"IOS_ES", "IOS - ETicket Services"};
  m_log[IOS_FS] = {"IOS_FS", "IOS - Filesystem Services"};
  m_log[IOS_SD] = {"IOS_SD", "IOS - SDIO"};
  m_log[IOS_SSL] = {"IOS_SSL", "IOS - SSL"};
  m_log[IOS_STM] = {"IOS_STM", "IOS - State Transition Manager"};
  m_log[IOS_NET] = {"IOS_NET", "IOS - Network"};
  m_log[IOS_USB] = {"IOS_USB", "IOS - USB"};
  m_log[IOS_WC24] = {"IOS_WC24", "IOS - WiiConnect24"};
  m_log[IOS_WFS] = {"IOS_WFS", "IOS - WFS"};
  m_log[IOS_WIIMOTE] = {"IOS_WIIMOTE", "IOS - Wii Remote"};
  m_log[MASTER_LOG] = {"MASTER", "Master Log"};
  m_log[MEMCARD_MANAGER] = {"MemCard Manager", "MemCard Manager"};
  m_log[MEMMAP] = {"MI", "MI & memmap"};
  m_log[NETPLAY] = {"NETPLAY", "Netplay"};
  m_log[OSHLE] = {"HLE", "HLE"};
  m_log[OSREPORT] = {"OSREPORT", "OSReport"};
  m_log[PAD] = {"PAD", "Pad"};
  m_log[PIXELENGINE] = {"PE", "PixelEngine"};
  m_log[PROCESSORINTERFACE] = {"PI", "ProcessorInt"};
  m_log[POWERPC] = {"PowerPC", "IBM CPU"};
  m_log[SERIALINTERFACE] = {"SI", "Serial Interface"};
  m_log[SP1] = {"SP1", "Serial Port 1"};
  m_log[SYMBOLS] = {"SYMBOLS", "Symbols"};
  m_log[VIDEO] = {"Video", "Video Backend"};
  m_log[VIDEOINTERFACE] = {"VI", "Video Interface"};
  m_log[WIIMOTE] = {"Wiimote", "Wiimote"};
  m_log[WII_IPC] = {"WII_IPC", "WII IPC"};

  RegisterListener(LogListener::FILE_LISTENER,
                   new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
  RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

  // Set up log listeners
  int verbosity = Config::Get(LOGGER_VERBOSITY);

  // Ensure the verbosity level is valid
  if (verbosity < 1)
    verbosity = 1;
  if (verbosity > MAX_LOGLEVEL)
    verbosity = MAX_LOGLEVEL;

  SetLogLevel(static_cast<LOG_LEVELS>(verbosity));
  EnableListener(LogListener::FILE_LISTENER, Config::Get(LOGGER_WRITE_TO_FILE));
  EnableListener(LogListener::CONSOLE_LISTENER, Config::Get(LOGGER_WRITE_TO_CONSOLE));
  EnableListener(LogListener::LOG_WINDOW_LISTENER, Config::Get(LOGGER_WRITE_TO_WINDOW));

  for (LogContainer& container : m_log)
    container.m_enable = Config::Get(
        Config::ConfigInfo<bool>{{Config::System::Logger, "Logs", container.m_short_name}, false});

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
  Config::SetBaseOrCurrent(LOGGER_VERBOSITY, static_cast<int>(GetLogLevel()));

  for (const auto& container : m_log)
  {
    const Config::ConfigInfo<bool> info{{Config::System::Logger, "Logs", container.m_short_name},
                                        false};
    Config::SetBaseOrCurrent(info, container.m_enable);
  }

  Config::Save();
}

void LogManager::Log(LOG_LEVELS level, LOG_TYPE type, const char* file, int line,
                     const char* format, va_list args)
{
  return LogWithFullPath(level, type, file + m_path_cutoff_point, line, format, args);
}

void LogManager::LogWithFullPath(LOG_LEVELS level, LOG_TYPE type, const char* file, int line,
                                 const char* format, va_list args)
{
  if (!IsEnabled(type, level) || !static_cast<bool>(m_listener_ids))
    return;

  char temp[MAX_MSGLEN];
  CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

  const std::string msg =
      fmt::format("{} {}:{} {}[{}]: {}\n", Common::Timer::GetTimeFormatted(), file, line,
                  LOG_LEVEL_TO_CHAR[static_cast<int>(level)], GetShortName(type), temp);

  for (auto listener_id : m_listener_ids)
    if (m_listeners[listener_id])
      m_listeners[listener_id]->Log(level, msg.c_str());
}

LOG_LEVELS LogManager::GetLogLevel() const
{
  return m_level;
}

void LogManager::SetLogLevel(LOG_LEVELS level)
{
  m_level = level;
}

void LogManager::SetEnable(LOG_TYPE type, bool enable)
{
  m_log[type].m_enable = enable;
}

bool LogManager::IsEnabled(LOG_TYPE type, LOG_LEVELS level) const
{
  return m_log[type].m_enable && GetLogLevel() >= level;
}

const char* LogManager::GetShortName(LOG_TYPE type) const
{
  return m_log[type].m_short_name;
}

const char* LogManager::GetFullName(LOG_TYPE type) const
{
  return m_log[type].m_full_name;
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
}  // namespace Common::Log
