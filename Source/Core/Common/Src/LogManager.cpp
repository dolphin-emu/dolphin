// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "LogManager.h"
#include "Timer.h"
void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
				const char *file, int line, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	LogManager::GetInstance()->Log(level, type, file, line, fmt, args);
	va_end(args);
}

LogManager *LogManager::m_logManager = NULL;

LogManager::LogManager() : logMutex(1) {
	// create log files
	m_Log[LogTypes::MASTER_LOG]			= new LogContainer("*",				"Master Log");
	m_Log[LogTypes::BOOT]				= new LogContainer("BOOT",			"Boot");
	m_Log[LogTypes::COMMON]				= new LogContainer("COMMON",		"Common");
	m_Log[LogTypes::DISCIO]				= new LogContainer("DIO",	    	"Disc IO");
	m_Log[LogTypes::FILEMON]			= new LogContainer("FileMon",		"File Monitor");
	m_Log[LogTypes::PAD]		        = new LogContainer("PAD",			"Pad");
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
	m_Log[LogTypes::HLE]				= new LogContainer("HLE",			"HLE");
	m_Log[LogTypes::DSPHLE]			    = new LogContainer("DSPHLE",		"DSP HLE");
	m_Log[LogTypes::DSPLLE]			    = new LogContainer("DSPLLE",		"DSP LLE");
	m_Log[LogTypes::DSP_MAIL]		    = new LogContainer("DSPMails",		"DSP Mails");
	m_Log[LogTypes::VIDEO]			    = new LogContainer("Video",			"Video Plugin");
	m_Log[LogTypes::AUDIO]			    = new LogContainer("Audio",			"Audio Plugin");
	m_Log[LogTypes::DYNA_REC]			= new LogContainer("JIT",			"Dynamic Recompiler");
	m_Log[LogTypes::CONSOLE]			= new LogContainer("CONSOLE",		"Dolphin Console");
	m_Log[LogTypes::OSREPORT]			= new LogContainer("OSREPORT",		"OSReport");			
	m_Log[LogTypes::WIIMOTE]			= new LogContainer("Wiimote",		"Wiimote Plugin");			
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
	m_Log[LogTypes::MEMCARD_MANAGER]	= new LogContainer("MemCard Manger", "MemCard Manger");
	m_Log[LogTypes::NETPLAY]			= new LogContainer("NETPLAY",		"Netplay");

	m_fileLog = new FileLogListener(MAIN_LOG_FILE);
	m_consoleLog = new ConsoleListener();

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i) {
		m_Log[i]->setEnable(true);
		m_Log[i]->addListener(m_fileLog);
		m_Log[i]->addListener(m_consoleLog);
	}
}

LogManager::~LogManager() {
	delete [] &m_Log;  // iffy :P
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i) {
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_consoleLog);
	}
	delete m_fileLog;
	delete m_consoleLog;
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
					 const char *file, int line, const char *format, 
					 va_list args) {

	char temp[MAX_MSGLEN];
	char msg[MAX_MSGLEN + 512];
	LogContainer *log = m_Log[type];

	if (! log->isEnable() || level > log->getLevel())
		return;

	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

	static const char level_to_char[7] = "-NEWID";
	sprintf(msg, "%s %s:%u %c[%s]: %s\n",
			Common::Timer::GetTimeFormatted().c_str(),
			file, line, level_to_char[(int)level],
			log->getShortName(), temp);

	logMutex.Enter();
	log->trigger(level, msg);
	logMutex.Leave();
}

void LogManager::removeListener(LogTypes::LOG_TYPE type, LogListener *listener) {
	logMutex.Enter();
	m_Log[type]->removeListener(listener);
	logMutex.Leave();
}

LogContainer::LogContainer(const char* shortName, const char* fullName, bool enable)
	: m_enable(enable) {
	strncpy(m_fullName, fullName, 128);
	strncpy(m_shortName, shortName, 32);
	m_level = LogTypes::LWARNING;
}

// LogContainer
void LogContainer::addListener(LogListener *listener) {
	if (!isListener(listener))
		listeners.push_back(listener);
}

void LogContainer::removeListener(LogListener *listener) {
	std::vector<LogListener *>::iterator i;
	for(i = listeners.begin(); i != listeners.end(); i++) {
		if ((*i) == listener) {
			listeners.erase(i);
			break;
		}
	}
}

bool LogContainer::isListener(LogListener *listener) const {
	std::vector<LogListener *>::const_iterator i;
	for (i = listeners.begin(); i != listeners.end(); i++) {
		if ((*i) == listener) {
			return true;
		}
	}
	return false;
}

void LogContainer::trigger(LogTypes::LOG_LEVELS level, const char *msg) {
	std::vector<LogListener *>::const_iterator i;
	for (i = listeners.begin(); i != listeners.end(); i++) {
		(*i)->Log(level, msg);
	}
}

FileLogListener::FileLogListener(const char *filename) {
	m_filename = strndup(filename, 255);
	m_logfile = fopen(filename, "a+");
	setEnable(true);
}

FileLogListener::~FileLogListener() {
	free(m_filename);
	fclose(m_logfile);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg) {
	if (!m_enable || !isValid())
		return;

   	fwrite(msg, strlen(msg) * sizeof(char), 1, m_logfile);
	fflush(m_logfile);
}
