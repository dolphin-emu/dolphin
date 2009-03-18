#include "LogManager.h"
#include "Timer.h"

LogManager *LogManager::m_logManager = NULL;

LogManager::LogManager() {
	// create log files
	m_Log[LogTypes::MASTER_LOG]			= new LogContainer("*",				"Master Log");
	m_Log[LogTypes::BOOT]				= new LogContainer("BOOT",			"Boot");
	m_Log[LogTypes::COMMON]				= new LogContainer("COMMON",		"Common");
	m_Log[LogTypes::DISCIO]				= new LogContainer("DIO",	    	"Disc IO");
	m_Log[LogTypes::PAD]		        = new LogContainer("PAD",			"Pad");
	m_Log[LogTypes::PIXELENGINE]		= new LogContainer("PE",			"PixelEngine");
	m_Log[LogTypes::COMMANDPROCESSOR]	= new LogContainer("CP",			"CommandProc");
	m_Log[LogTypes::VIDEOINTERFACE]		= new LogContainer("VI",			"VideoInt");
	m_Log[LogTypes::SERIALINTERFACE]	= new LogContainer("SI",			"SerialInt");
	m_Log[LogTypes::PERIPHERALINTERFACE]= new LogContainer("PI",			"PeripheralInt");
	m_Log[LogTypes::MEMMAP]				= new LogContainer("MI",			"MI & memmap");
	m_Log[LogTypes::STREAMINGINTERFACE] = new LogContainer("Stream",		"StreamingInt");
	m_Log[LogTypes::DSPINTERFACE]		= new LogContainer("DSP",			"DSPInterface");
	m_Log[LogTypes::DVDINTERFACE]		= new LogContainer("DVD",			"DVDInterface");
	m_Log[LogTypes::GPFIFO]				= new LogContainer("GP",			"GPFifo");
	m_Log[LogTypes::EXPANSIONINTERFACE]	= new LogContainer("EXI",			"ExpansionInt");
	m_Log[LogTypes::AUDIO_INTERFACE]	= new LogContainer("AI",			"AudioInt");
	m_Log[LogTypes::GEKKO]				= new LogContainer("GEKKO",			"IBM CPU");
	m_Log[LogTypes::HLE]				= new LogContainer("HLE",			"HLE");
	m_Log[LogTypes::DSPHLE]			    = new LogContainer("DSPHLE",		"DSP HLE");
	m_Log[LogTypes::VIDEO]			    = new LogContainer("Video",			"Video Plugin");
	m_Log[LogTypes::AUDIO]			    = new LogContainer("Audio",			"Audio Plugin");
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
	m_Log[LogTypes::WII_IPC_NET]		= new LogContainer("WII_IPC_NET",	"WII IPC NET");
	m_Log[LogTypes::WII_IPC_WIIMOTE]    = new LogContainer("WII_IPC_WIIMOTE","WII IPC WIIMOTE");
	m_Log[LogTypes::ACTIONREPLAY]       = new LogContainer("ActionReplay",	"ActionReplay");

	logMutex = new Common::CriticalSection(1);
	m_fileLog = new FileLogListener(MAIN_LOG_FILE);
	m_consoleLog = new ConsoleListener();

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i) {
		m_Log[i]->setEnable(true);
		m_Log[i]->addListener(m_fileLog);
		m_Log[i]->addListener(m_consoleLog);
	}
}

LogManager::~LogManager() {
	delete [] &m_Log;
	delete logMutex;
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i) {
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_consoleLog);
	}
	delete m_fileLog;
	delete m_consoleLog;
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
					 const char *format, ...) {
	va_list args;

	char temp[MAX_MSGLEN];
	char msg[MAX_MSGLEN + 512];
	LogContainer *log = m_Log[type];

	if (! log->isEnable() || level > log->getLevel())
		return;
	
	va_start(args, format);
	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);
	va_end(args);
	sprintf(msg, "%s: %i %s %s\n",
			Common::Timer::GetTimeFormatted().c_str(),
			//			PowerPC::ppcState.DebugCount,
			(int)level,
			log->getShortName(),
			temp);

	logMutex->Enter();
	log->trigger(level, msg);
	logMutex->Leave();

}

void LogManager::removeListener(LogTypes::LOG_TYPE type, Listener *listener) {
	logMutex->Enter();
	m_Log[type]->removeListener(listener);
	logMutex->Leave();
}

// LogContainer
void LogContainer::addListener(Listener *listener) {
	std::vector<Listener *>::iterator i;
	bool exists = false;

	for(i=listeners.begin();i!=listeners.end();i++) {
		if ((*i) == listener) {
			exists = true;
			break;
		}
	}

	if (! exists)
		listeners.push_back(listener);
}

void LogContainer::removeListener(Listener *listener) {
	std::vector<Listener *>::iterator i;
	for(i=listeners.begin();i!=listeners.end();i++) {
		if ((*i) == listener) {
			listeners.erase(i);
			break;
		}
	}
}

bool LogContainer::isListener(Listener *listener) {
	std::vector<Listener *>::iterator i;
	for(i=listeners.begin();i!=listeners.end();i++) {
		if ((*i) == listener) {
			return true;
		}
	}
	return false;
}

void LogContainer::trigger(LogTypes::LOG_LEVELS level, const char *msg) {
	std::vector<Listener *>::const_iterator i;
	for(i=listeners.begin();i!=listeners.end();i++) {
		(*i)->Log(level, msg);
	}
}

FileLogListener::FileLogListener(const char *filename) : Listener("File") {
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

   	fwrite(msg, (strlen(msg) + 1) * sizeof(char), 1, m_logfile);
	fflush(m_logfile);
}
