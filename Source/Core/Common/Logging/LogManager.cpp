// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstring>
#include <mutex>
#include <ostream>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

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
#if !(defined(_MSC_VER) && _MSC_VER <= 1800)
  constexpr 
#endif
    const char* pattern = DIR_SEP "Source" DIR_SEP "Core" DIR_SEP;
  size_t pos = std::string(__FILE__).find(pattern);
  if (pos != std::string::npos)
    return pos + strlen(pattern);
  return 0;
}

LogManager::LogManager()
{
  // create log containers
  m_log[LogTypes::ACTIONREPLAY] = {"ActionReplay", "ActionReplay"};
  m_log[LogTypes::AUDIO] = {"Audio", "Audio Emulator"};
  m_log[LogTypes::AUDIO_INTERFACE] = {"AI", "Audio Interface (AI)"};
  m_log[LogTypes::BOOT] = {"BOOT", "Boot"};
  m_log[LogTypes::COMMANDPROCESSOR] = {"CP", "CommandProc"};
  m_log[LogTypes::COMMON] = {"COMMON", "Common"};
  m_log[LogTypes::CONSOLE] = {"CONSOLE", "Dolphin Console"};
  m_log[LogTypes::CORE] = {"CORE", "Core"};
  m_log[LogTypes::DISCIO] = {"DIO", "Disc IO"};
  m_log[LogTypes::DSPHLE] = {"DSPHLE", "DSP HLE"};
  m_log[LogTypes::DSPLLE] = {"DSPLLE", "DSP LLE"};
  m_log[LogTypes::DSP_MAIL] = {"DSPMails", "DSP Mails"};
  m_log[LogTypes::DSPINTERFACE] = {"DSP", "DSPInterface"};
  m_log[LogTypes::DVDINTERFACE] = {"DVD", "DVD Interface"};
  m_log[LogTypes::DYNA_REC] = {"JIT", "Dynamic Recompiler"};
  m_log[LogTypes::EXPANSIONINTERFACE] = {"EXI", "Expansion Interface"};
  m_log[LogTypes::FILEMON] = {"FileMon", "File Monitor"};
  m_log[LogTypes::GDB_STUB] = {"GDB_STUB", "GDB Stub"};
  m_log[LogTypes::GPFIFO] = {"GP", "GPFifo"};
  m_log[LogTypes::HOST_GPU] = {"Host GPU", "Host GPU"};
  m_log[LogTypes::IOS] = {"IOS", "IOS"};
  m_log[LogTypes::IOS_DI] = {"IOS_DI", "IOS - Drive Interface"};
  m_log[LogTypes::IOS_ES] = {"IOS_ES", "IOS - ETicket Services"};
  m_log[LogTypes::IOS_FILEIO] = {"IOS_FILEIO", "IOS - FileIO"};
  m_log[LogTypes::IOS_SD] = {"IOS_SD", "IOS - SDIO"};
  m_log[LogTypes::IOS_SSL] = {"IOS_SSL", "IOS - SSL"};
  m_log[LogTypes::IOS_STM] = {"IOS_STM", "IOS - State Transition Manager"};
  m_log[LogTypes::IOS_NET] = {"IOS_NET", "IOS - Network"};
  m_log[LogTypes::IOS_USB] = {"IOS_USB", "IOS - USB"};
  m_log[LogTypes::IOS_WC24] = {"IOS_WC24", "IOS - WiiConnect24"};
  m_log[LogTypes::IOS_WFS] = {"IOS_WFS", "IOS - WFS"};
  m_log[LogTypes::IOS_WIIMOTE] = {"IOS_WIIMOTE", "IOS - Wii Remote"};
  m_log[LogTypes::MASTER_LOG] = {"*", "Master Log"};
  m_log[LogTypes::MEMCARD_MANAGER] = {"MemCard Manager", "MemCard Manager"};
  m_log[LogTypes::MEMMAP] = {"MI", "MI & memmap"};
  m_log[LogTypes::NETPLAY] = {"NETPLAY", "Netplay"};
  m_log[LogTypes::OSHLE] = {"HLE", "HLE"};
  m_log[LogTypes::OSREPORT] = {"OSREPORT", "OSReport"};
  m_log[LogTypes::PAD] = {"PAD", "Pad"};
  m_log[LogTypes::PIXELENGINE] = {"PE", "PixelEngine"};
  m_log[LogTypes::PROCESSORINTERFACE] = {"PI", "ProcessorInt"};
  m_log[LogTypes::POWERPC] = {"PowerPC", "IBM CPU"};
  m_log[LogTypes::SERIALINTERFACE] = {"SI", "Serial Interface (SI)"};
  m_log[LogTypes::SP1] = {"SP1", "Serial Port 1"};
  m_log[LogTypes::VIDEO] = {"Video", "Video Backend"};
  m_log[LogTypes::VIDEOINTERFACE] = {"VI", "Video Interface (VI)"};
  m_log[LogTypes::VR] = {"VR", "Virtual Reality"};
  m_log[LogTypes::WIIMOTE] = {"Wiimote", "Wiimote"};
  m_log[LogTypes::WII_IPC] = {"WII_IPC", "WII IPC"};

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

  SetLogLevel(static_cast<LogTypes::LOG_LEVELS>(verbosity));
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
  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_FILE, IsListenerEnabled(LogListener::FILE_LISTENER));
  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_CONSOLE,
                           IsListenerEnabled(LogListener::CONSOLE_LISTENER));
  Config::SetBaseOrCurrent(LOGGER_WRITE_TO_WINDOW,
                           IsListenerEnabled(LogListener::LOG_WINDOW_LISTENER));
  Config::SetBaseOrCurrent(LOGGER_VERBOSITY, static_cast<int>(GetLogLevel()));

  for (const auto& container : m_log)
    Config::SetBaseOrCurrent({{Config::System::Logger, "Logs", container.m_short_name}, false},
                             container.m_enable);

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

  std::string msg =
      StringFromFormat("%s %s:%u %c[%s]: %s\n", Common::Timer::GetTimeFormatted().c_str(), file,
                       line, LogTypes::LOG_LEVEL_TO_CHAR[(int)level], GetShortName(type), temp);

  for (auto listener_id : m_listener_ids)
    if (m_listeners[listener_id])
      m_listeners[listener_id]->Log(level, msg.c_str());
}

LogTypes::LOG_LEVELS LogManager::GetLogLevel() const
{
  return m_level;
}

void LogManager::SetLogLevel(LogTypes::LOG_LEVELS level)
{
  m_level = level;
}

void LogManager::SetEnable(LogTypes::LOG_TYPE type, bool enable)
{
  m_log[type].m_enable = enable;
}

bool LogManager::IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level) const
{
  return m_log[type].m_enable && GetLogLevel() >= level;
}

const char* LogManager::GetShortName(LogTypes::LOG_TYPE type) const
{
  return m_log[type].m_short_name;
}

const char* LogManager::GetFullName(LogTypes::LOG_TYPE type) const
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
