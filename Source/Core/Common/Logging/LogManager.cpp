// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstring>
#include <mutex>
#include <ostream>
#include <set>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
                const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  if (LogManager::GetInstance())
    LogManager::GetInstance()->Log(level, type, file, line, fmt, args);
  va_end(args);
}

LogManager* LogManager::m_logManager = nullptr;

static size_t DeterminePathCutOffPoint()
{
  constexpr const char* pattern = DIR_SEP "Source" DIR_SEP "Core" DIR_SEP;
  size_t pos = std::string(__FILE__).find(pattern);
  if (pos != std::string::npos)
    return pos + strlen(pattern);
  return 0;
}

LogManager::LogManager()
{
  // create log containers
  m_Log[LogTypes::ACTIONREPLAY] = new LogContainer("ActionReplay", "ActionReplay");
  m_Log[LogTypes::AUDIO] = new LogContainer("Audio", "Audio Emulator");
  m_Log[LogTypes::AUDIO_INTERFACE] = new LogContainer("AI", "Audio Interface (AI)");
  m_Log[LogTypes::BOOT] = new LogContainer("BOOT", "Boot");
  m_Log[LogTypes::COMMANDPROCESSOR] = new LogContainer("CP", "CommandProc");
  m_Log[LogTypes::COMMON] = new LogContainer("COMMON", "Common");
  m_Log[LogTypes::CONSOLE] = new LogContainer("CONSOLE", "Dolphin Console");
  m_Log[LogTypes::DISCIO] = new LogContainer("DIO", "Disc IO");
  m_Log[LogTypes::DSPHLE] = new LogContainer("DSPHLE", "DSP HLE");
  m_Log[LogTypes::DSPLLE] = new LogContainer("DSPLLE", "DSP LLE");
  m_Log[LogTypes::DSP_MAIL] = new LogContainer("DSPMails", "DSP Mails");
  m_Log[LogTypes::DSPINTERFACE] = new LogContainer("DSP", "DSPInterface");
  m_Log[LogTypes::DVDINTERFACE] = new LogContainer("DVD", "DVD Interface");
  m_Log[LogTypes::DYNA_REC] = new LogContainer("JIT", "Dynamic Recompiler");
  m_Log[LogTypes::EXPANSIONINTERFACE] = new LogContainer("EXI", "Expansion Interface");
  m_Log[LogTypes::FILEMON] = new LogContainer("FileMon", "File Monitor");
  m_Log[LogTypes::GDB_STUB] = new LogContainer("GDB_STUB", "GDB Stub");
  m_Log[LogTypes::GPFIFO] = new LogContainer("GP", "GPFifo");
  m_Log[LogTypes::HOST_GPU] = new LogContainer("Host GPU", "Host GPU");
  m_Log[LogTypes::IOS] = new LogContainer("IOS", "IOS");
  m_Log[LogTypes::IOS_DI] = new LogContainer("IOS_DI", "IOS - Drive Interface");
  m_Log[LogTypes::IOS_ES] = new LogContainer("IOS_ES", "IOS - ETicket Services");
  m_Log[LogTypes::IOS_FILEIO] = new LogContainer("IOS_FILEIO", "IOS - FileIO");
  m_Log[LogTypes::IOS_SD] = new LogContainer("IOS_SD", "IOS - SDIO");
  m_Log[LogTypes::IOS_SSL] = new LogContainer("IOS_SSL", "IOS - SSL");
  m_Log[LogTypes::IOS_STM] = new LogContainer("IOS_STM", "IOS - State Transition Manager");
  m_Log[LogTypes::IOS_NET] = new LogContainer("IOS_NET", "IOS - Network");
  m_Log[LogTypes::IOS_USB] = new LogContainer("IOS_USB", "IOS - USB");
  m_Log[LogTypes::IOS_WC24] = new LogContainer("IOS_WC24", "IOS - WiiConnect24");
  m_Log[LogTypes::IOS_WIIMOTE] = new LogContainer("IOS_WIIMOTE", "IOS - Wii Remote");
  m_Log[LogTypes::MASTER_LOG] = new LogContainer("*", "Master Log");
  m_Log[LogTypes::MEMCARD_MANAGER] = new LogContainer("MemCard Manager", "MemCard Manager");
  m_Log[LogTypes::MEMMAP] = new LogContainer("MI", "MI & memmap");
  m_Log[LogTypes::NETPLAY] = new LogContainer("NETPLAY", "Netplay");
  m_Log[LogTypes::OSHLE] = new LogContainer("HLE", "HLE");
  m_Log[LogTypes::OSREPORT] = new LogContainer("OSREPORT", "OSReport");
  m_Log[LogTypes::PAD] = new LogContainer("PAD", "Pad");
  m_Log[LogTypes::PIXELENGINE] = new LogContainer("PE", "PixelEngine");
  m_Log[LogTypes::PROCESSORINTERFACE] = new LogContainer("PI", "ProcessorInt");
  m_Log[LogTypes::POWERPC] = new LogContainer("PowerPC", "IBM CPU");
  m_Log[LogTypes::SERIALINTERFACE] = new LogContainer("SI", "Serial Interface (SI)");
  m_Log[LogTypes::SP1] = new LogContainer("SP1", "Serial Port 1");
  m_Log[LogTypes::VIDEO] = new LogContainer("Video", "Video Backend");
  m_Log[LogTypes::VIDEOINTERFACE] = new LogContainer("VI", "Video Interface (VI)");
  m_Log[LogTypes::WIIMOTE] = new LogContainer("Wiimote", "Wiimote");
  m_Log[LogTypes::WII_IPC] = new LogContainer("WII_IPC", "WII IPC");

  RegisterListener(LogListener::FILE_LISTENER,
                   new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
  RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

  IniFile ini;
  ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));
  IniFile::Section* logs = ini.GetOrCreateSection("Logs");
  IniFile::Section* options = ini.GetOrCreateSection("Options");
  bool write_file;
  bool write_console;
  options->Get("WriteToFile", &write_file, false);
  options->Get("WriteToConsole", &write_console, true);

  for (LogContainer* container : m_Log)
  {
    bool enable;
    logs->Get(container->GetShortName(), &enable, false);
    container->SetEnable(enable);
    if (enable && write_file)
      container->AddListener(LogListener::FILE_LISTENER);
    if (enable && write_console)
      container->AddListener(LogListener::CONSOLE_LISTENER);
  }

  m_path_cutoff_point = DeterminePathCutOffPoint();
}

LogManager::~LogManager()
{
  for (LogContainer* container : m_Log)
    delete container;

  // The log window listener pointer is owned by the GUI code.
  delete m_listeners[LogListener::CONSOLE_LISTENER];
  delete m_listeners[LogListener::FILE_LISTENER];
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
                     int line, const char* format, va_list args)
{
  char temp[MAX_MSGLEN];
  LogContainer* log = m_Log[type];

  if (!log->IsEnabled() || level > log->GetLevel() || !log->HasListeners())
    return;

  CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

  const char* path_to_print = file + m_path_cutoff_point;

  std::string msg = StringFromFormat(
      "%s %s:%u %c[%s]: %s\n", Common::Timer::GetTimeFormatted().c_str(), path_to_print, line,
      LogTypes::LOG_LEVEL_TO_CHAR[(int)level], log->GetShortName().c_str(), temp);

  for (auto listener_id : *log)
    m_listeners[listener_id]->Log(level, msg.c_str());
}

void LogManager::Init()
{
  m_logManager = new LogManager();
}

void LogManager::Shutdown()
{
  delete m_logManager;
  m_logManager = nullptr;
}

LogContainer::LogContainer(const std::string& shortName, const std::string& fullName, bool enable)
    : m_fullName(fullName), m_shortName(shortName), m_enable(enable), m_level(LogTypes::LWARNING)
{
}

FileLogListener::FileLogListener(const std::string& filename)
{
  OpenFStream(m_logfile, filename, std::ios::app);
  SetEnable(true);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char* msg)
{
  if (!IsEnabled() || !IsValid())
    return;

  std::lock_guard<std::mutex> lk(m_log_lock);
  m_logfile << msg << std::flush;
}
