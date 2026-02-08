// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Logging/LogManager.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <locale>
#include <mutex>
#include <ostream>
#include <string>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace Common::Log
{
const Config::Info<bool> LOGGER_WRITE_TO_FILE{{Config::System::Logger, "Options", "WriteToFile"},
                                              false};
const Config::Info<bool> LOGGER_WRITE_TO_CONSOLE{
    {Config::System::Logger, "Options", "WriteToConsole"}, true};
const Config::Info<bool> LOGGER_WRITE_TO_WINDOW{
    {Config::System::Logger, "Options", "WriteToWindow"}, true};
const Config::Info<LogLevel> LOGGER_VERBOSITY{{Config::System::Logger, "Options", "Verbosity"},
                                              LogLevel::LNOTICE};

class FileLogListener : public LogListener
{
public:
  FileLogListener(const std::string& filename)
  {
    File::OpenFStream(m_logfile, filename, std::ios::app);
    SetEnable(true);
  }

  void Log(LogLevel, const char* msg) override
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

void GenericLogFmtImpl(LogLevel level, LogType type, const char* file, int line,
                       fmt::string_view format, const fmt::format_args& args)
{
  auto* instance = LogManager::GetInstance();
  if (instance == nullptr)
    return;

  if (!instance->IsEnabled(type, level))
    return;

  const auto message = fmt::vformat(format, args);
  instance->Log(level, type, file, line, message.c_str());
}

static size_t DeterminePathCutOffPoint()
{
  constexpr const char* pattern = "/source/core/";
#ifdef _WIN32
  constexpr const char* pattern2 = "\\source\\core\\";
#endif
  std::string path = __FILE__;
  Common::ToLower(&path);
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
  m_log[LogType::ACHIEVEMENTS] = {"RetroAchievements", "Achievements"};
  m_log[LogType::ACTIONREPLAY] = {"ActionReplay", "Action Replay"};
  m_log[LogType::AUDIO] = {"Audio", "Audio Emulator"};
  m_log[LogType::AUDIO_INTERFACE] = {"AI", "Audio Interface"};
  m_log[LogType::BOOT] = {"BOOT", "Boot"};
  m_log[LogType::COMMANDPROCESSOR] = {"CP", "Command Processor"};
  m_log[LogType::COMMON] = {"COMMON", "Common"};
  m_log[LogType::CONSOLE] = {"CONSOLE", "Dolphin Console"};
  m_log[LogType::CONTROLLERINTERFACE] = {"CI", "Controller Interface"};
  m_log[LogType::CORE] = {"CORE", "Core"};
  m_log[LogType::DISCIO] = {"DIO", "Disc IO"};
  m_log[LogType::DSPHLE] = {"DSPHLE", "DSP HLE"};
  m_log[LogType::DSPLLE] = {"DSPLLE", "DSP LLE"};
  m_log[LogType::DSP_MAIL] = {"DSPMails", "DSP Mails"};
  m_log[LogType::DSPINTERFACE] = {"DSP", "DSP Interface"};
  m_log[LogType::DVDINTERFACE] = {"DVD", "DVD Interface"};
  m_log[LogType::DYNA_REC] = {"JIT", "JIT Dynamic Recompiler"};
  m_log[LogType::EXPANSIONINTERFACE] = {"EXI", "Expansion Interface"};
  m_log[LogType::FILEMON] = {"FileMon", "File Monitor"};
  m_log[LogType::FRAMEDUMP] = {"FRAMEDUMP", "FrameDump"};
  m_log[LogType::GDB_STUB] = {"GDB_STUB", "GDB Stub"};
  m_log[LogType::GPFIFO] = {"GP", "GatherPipe FIFO"};
  m_log[LogType::HOST_GPU] = {"Host GPU", "Host GPU"};
  m_log[LogType::HSP] = {"HSP", "High-Speed Port (HSP)"};
  m_log[LogType::IOS] = {"IOS", "IOS"};
  m_log[LogType::IOS_DI] = {"IOS_DI", "IOS - Drive Interface"};
  m_log[LogType::IOS_ES] = {"IOS_ES", "IOS - ETicket Services"};
  m_log[LogType::IOS_FS] = {"IOS_FS", "IOS - Filesystem Services"};
  m_log[LogType::IOS_SD] = {"IOS_SD", "IOS - SDIO"};
  m_log[LogType::IOS_SSL] = {"IOS_SSL", "IOS - SSL"};
  m_log[LogType::IOS_STM] = {"IOS_STM", "IOS - State Transition Manager"};
  m_log[LogType::IOS_NET] = {"IOS_NET", "IOS - Network"};
  m_log[LogType::IOS_USB] = {"IOS_USB", "IOS - USB"};
  m_log[LogType::IOS_WC24] = {"IOS_WC24", "IOS - WiiConnect24"};
  m_log[LogType::IOS_WFS] = {"IOS_WFS", "IOS - WFS"};
  m_log[LogType::IOS_WIIMOTE] = {"IOS_WIIMOTE", "IOS - Wii Remote"};
  m_log[LogType::MASTER_LOG] = {"MASTER", "Master Log"};
  m_log[LogType::MEMCARD_MANAGER] = {"MemCard Manager", "Memory Card Manager"};
  m_log[LogType::MEMMAP] = {"MI", "Memory Interface & Memory Map"};
  m_log[LogType::NETPLAY] = {"NETPLAY", "Netplay"};
  m_log[LogType::OSHLE] = {"HLE", "OSHLE"};
  m_log[LogType::OSREPORT] = {"OSREPORT", "OSReport EXI"};
  m_log[LogType::OSREPORT_HLE] = {"OSREPORT_HLE", "OSReport HLE"};
  m_log[LogType::PIXELENGINE] = {"PE", "Pixel Engine"};
  m_log[LogType::PROCESSORINTERFACE] = {"PI", "Processor Interface"};
  m_log[LogType::POWERPC] = {"PowerPC", "PowerPC IBM CPU"};
  m_log[LogType::SERIALINTERFACE] = {"SI", "Serial Interface"};
  m_log[LogType::SP1] = {"SP1", "Serial Port 1"};
  m_log[LogType::SYMBOLS] = {"SYMBOLS", "Symbols"};
  m_log[LogType::VIDEO] = {"Video", "Video Backend"};
  m_log[LogType::VIDEOINTERFACE] = {"VI", "Video Interface"};
  m_log[LogType::WIIMOTE] = {"Wiimote", "Wii Remote"};
  m_log[LogType::WII_IPC] = {"WII_IPC", "WII IPC"};

  RegisterListener(LogListener::FILE_LISTENER,
                   new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
  RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

  // Set up log listeners
  LogLevel verbosity = Config::Get(LOGGER_VERBOSITY);

  SetLogLevel(verbosity);
  EnableListener(LogListener::FILE_LISTENER, Config::Get(LOGGER_WRITE_TO_FILE));
  EnableListener(LogListener::CONSOLE_LISTENER, Config::Get(LOGGER_WRITE_TO_CONSOLE));
  EnableListener(LogListener::LOG_WINDOW_LISTENER, Config::Get(LOGGER_WRITE_TO_WINDOW));

  for (auto& container : m_log)
  {
    container.m_enable = Config::Get(
        Config::Info<bool>{{Config::System::Logger, "Logs", container.m_short_name}, false});
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
  Config::SetBaseOrCurrent(LOGGER_VERBOSITY, GetLogLevel());

  for (const auto& container : m_log)
  {
    const Config::Info<bool> info{{Config::System::Logger, "Logs", container.m_short_name}, false};
    Config::SetBaseOrCurrent(info, container.m_enable);
  }

  Config::Save();
}

void LogManager::Log(LogLevel level, LogType type, const char* file, int line, const char* message)
{
  if (!IsEnabled(type, level) || !static_cast<bool>(m_listener_ids))
    return;

  LogWithFullPath(level, type, file + m_path_cutoff_point, line, message);
}

std::string LogManager::GetTimestamp()
{
  // NOTE: the Qt LogWidget hardcodes the expected length of the timestamp portion of the log line,
  // so ensure they stay in sync

  // We want milliseconds *and not hours*, so can't directly use STL formatters
  const auto now = std::chrono::system_clock::now();
  const auto now_s = std::chrono::floor<std::chrono::seconds>(now);
  const auto now_ms = std::chrono::floor<std::chrono::milliseconds>(now);
  return fmt::format("{:%M:%S}:{:03}", now_s, (now_ms - now_s).count());
}

void LogManager::LogWithFullPath(LogLevel level, LogType type, const char* file, int line,
                                 const char* message)
{
  const std::string msg =
      fmt::format("{} {}:{} {}[{}]: {}\n", GetTimestamp(), file, line,
                  LOG_LEVEL_TO_CHAR[static_cast<int>(level)], GetShortName(type), message);

  for (const auto listener_id : m_listener_ids)
  {
    if (m_listeners[listener_id])
      m_listeners[listener_id]->Log(level, msg.c_str());
  }
}

LogLevel LogManager::GetLogLevel() const
{
  return m_level;
}

void LogManager::SetLogLevel(LogLevel level)
{
  m_level = std::clamp(level, LogLevel::LNOTICE, MAX_LOGLEVEL);
}

void LogManager::SetEnable(LogType type, bool enable)
{
  m_log[type].m_enable = enable;
}

bool LogManager::IsEnabled(LogType type, LogLevel level) const
{
  return m_log[type].m_enable && GetLogLevel() >= level;
}

std::vector<LogManager::LogContainer> LogManager::GetLogTypes()
{
  std::vector<LogContainer> log_types;
  log_types.reserve(m_log.size());

  for (const auto& container : m_log)
    log_types.emplace_back(container);

  return log_types;
}

const char* LogManager::GetShortName(LogType type) const
{
  return m_log[type].m_short_name;
}

const char* LogManager::GetFullName(LogType type) const
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
