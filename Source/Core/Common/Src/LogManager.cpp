// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#ifdef ANDROID
#include "Host.h"
#endif
#include "LogManager.h"
#include "ConsoleListener.h"
#include "Timer.h"
#include "Thread.h"
#include "FileUtil.h"

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
		const char *file, int line, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (LogManager::GetInstance())
		LogManager::GetInstance()->Log(level, type,
			file, line, fmt, args);
	va_end(args);
}

LogManager *LogManager::m_logManager = NULL;

LogManager::LogManager()
{
	// create log files
	m_Log[LogTypes::MASTER_LOG]			= new LogContainer("*",				"Master Log");
	m_Log[LogTypes::BOOT]				= new LogContainer("BOOT",			"Boot");
	m_Log[LogTypes::COMMON]				= new LogContainer("COMMON",		"Common");
	m_Log[LogTypes::DISCIO]				= new LogContainer("DIO",			"Disc IO");
	m_Log[LogTypes::FILEMON]			= new LogContainer("FileMon",		"File Monitor");
	m_Log[LogTypes::PAD]				= new LogContainer("PAD",			"Pad");
	m_Log[LogTypes::PIXELENGINE]		= new LogContainer("PE",			"PixelEngine");
	m_Log[LogTypes::COMMANDPROCESSOR]	= new LogContainer("CP",			"CommandProc");
	m_Log[LogTypes::VIDEOINTERFACE]		= new LogContainer("VI",			"VideoInt");
	m_Log[LogTypes::SERIALINTERFACE]	= new LogContainer("SI",			"SerialInt");
	m_Log[LogTypes::PROCESSORINTERFACE]	= new LogContainer("PI",			"ProcessorInt");
	m_Log[LogTypes::MEMMAP]				= new LogContainer("MI",			"MI & memmap");
	m_Log[LogTypes::SP1]				= new LogContainer("SP1",			"Serial Port 1");
	m_Log[LogTypes::STREAMINGINTERFACE] = new LogContainer("Stream",		"StreamingInt");
	m_Log[LogTypes::DSPINTERFACE]		= new LogContainer("DSP",			"DSPInterface");
	m_Log[LogTypes::DVDINTERFACE]		= new LogContainer("DVD",			"DVDInterface");
	m_Log[LogTypes::GPFIFO]				= new LogContainer("GP",			"GPFifo");
	m_Log[LogTypes::EXPANSIONINTERFACE]	= new LogContainer("EXI",			"ExpansionInt");
	m_Log[LogTypes::AUDIO_INTERFACE]	= new LogContainer("AI",			"AudioInt");
	m_Log[LogTypes::POWERPC]			= new LogContainer("PowerPC",		"IBM CPU");
	m_Log[LogTypes::OSHLE]				= new LogContainer("HLE",			"HLE");
	m_Log[LogTypes::DSPHLE]				= new LogContainer("DSPHLE",		"DSP HLE");
	m_Log[LogTypes::DSPLLE]				= new LogContainer("DSPLLE",		"DSP LLE");
	m_Log[LogTypes::DSP_MAIL]			= new LogContainer("DSPMails",		"DSP Mails");
	m_Log[LogTypes::VIDEO]				= new LogContainer("Video",			"Video Backend");
	m_Log[LogTypes::AUDIO]				= new LogContainer("Audio",			"Audio Emulator");
	m_Log[LogTypes::DYNA_REC]			= new LogContainer("JIT",			"Dynamic Recompiler");
	m_Log[LogTypes::CONSOLE]			= new LogContainer("CONSOLE",		"Dolphin Console");
	m_Log[LogTypes::OSREPORT]			= new LogContainer("OSREPORT",		"OSReport");
	m_Log[LogTypes::WIIMOTE]			= new LogContainer("Wiimote",		"Wiimote");
	m_Log[LogTypes::WII_IOB]			= new LogContainer("WII_IOB",		"WII IO Bridge");
	m_Log[LogTypes::WII_IPC]			= new LogContainer("WII_IPC",		"WII IPC");
	m_Log[LogTypes::WII_IPC_HLE]		= new LogContainer("WII_IPC_HLE",	"WII IPC HLE");
	m_Log[LogTypes::WII_IPC_DVD]		= new LogContainer("WII_IPC_DVD",	"WII IPC DVD");
	m_Log[LogTypes::WII_IPC_ES]			= new LogContainer("WII_IPC_ES",	"WII IPC ES");
	m_Log[LogTypes::WII_IPC_FILEIO]		= new LogContainer("WII_IPC_FILEIO","WII IPC FILEIO");
	m_Log[LogTypes::WII_IPC_SD]			= new LogContainer("WII_IPC_SD",	"WII IPC SD");
	m_Log[LogTypes::WII_IPC_STM]		= new LogContainer("WII_IPC_STM",	"WII IPC STM");
	m_Log[LogTypes::WII_IPC_NET]		= new LogContainer("WII_IPC_NET",	"WII IPC NET");
	m_Log[LogTypes::WII_IPC_WIIMOTE]	= new LogContainer("WII_IPC_WIIMOTE","WII IPC WIIMOTE");
	m_Log[LogTypes::ACTIONREPLAY]		= new LogContainer("ActionReplay",	"ActionReplay");
	m_Log[LogTypes::MEMCARD_MANAGER]	= new LogContainer("MemCard Manager", "MemCard Manager");
	m_Log[LogTypes::NETPLAY]			= new LogContainer("NETPLAY",		"Netplay");

	m_fileLog = new FileLogListener(File::GetUserPath(F_MAINLOG_IDX).c_str());
	m_consoleLog = new ConsoleListener();
	m_debuggerLog = new DebuggerLogListener();

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_Log[i]->SetEnable(true);
		m_Log[i]->AddListener(m_fileLog);
		m_Log[i]->AddListener(m_consoleLog);
#ifdef _MSC_VER
		if (IsDebuggerPresent())
			m_Log[i]->AddListener(m_debuggerLog);
#endif
	}
}

LogManager::~LogManager()
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_fileLog);
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_consoleLog);
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_debuggerLog);
	}

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		delete m_Log[i];

	delete m_fileLog;
	delete m_consoleLog;
	delete m_debuggerLog;
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
	const char *file, int line, const char *format, va_list args)
{
	char temp[MAX_MSGLEN];
	char msg[MAX_MSGLEN * 2];
	LogContainer *log = m_Log[type];

	if (!log->IsEnabled() || level > log->GetLevel() || ! log->HasListeners())
		return;

	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

	static const char level_to_char[7] = "-NEWID";
	sprintf(msg, "%s %s:%u %c[%s]: %s\n",
		Common::Timer::GetTimeFormatted().c_str(),
		file, line, level_to_char[(int)level],
		log->GetShortName(), temp);
#ifdef ANDROID
	Host_SysMessage(msg);	
#endif
	log->Trigger(level, msg);
}

void LogManager::Init()
{
	m_logManager = new LogManager();
}

void LogManager::Shutdown()
{
	delete m_logManager;
	m_logManager = NULL;
}

LogContainer::LogContainer(const char* shortName, const char* fullName, bool enable)
	: m_enable(enable)
{
	strncpy(m_fullName, fullName, 128);
	strncpy(m_shortName, shortName, 32);
	m_level = LogTypes::LWARNING;
}

// LogContainer
void LogContainer::AddListener(LogListener *listener)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);
	m_listeners.insert(listener);
}

void LogContainer::RemoveListener(LogListener *listener)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);
	m_listeners.erase(listener);
}

void LogContainer::Trigger(LogTypes::LOG_LEVELS level, const char *msg)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);

	std::set<LogListener*>::const_iterator i;
	for (i = m_listeners.begin(); i != m_listeners.end(); ++i)
	{
		(*i)->Log(level, msg);
	}
}

FileLogListener::FileLogListener(const char *filename)
{
	OpenFStream(m_logfile, filename, std::ios::app);
	SetEnable(true);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
	if (!IsEnabled() || !IsValid())
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_logfile << msg << std::flush;
}

void DebuggerLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
#if _MSC_VER
	::OutputDebugStringA(msg);
#endif
}
