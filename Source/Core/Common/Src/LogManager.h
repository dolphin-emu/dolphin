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

#ifndef _LOGMANAGER_H_
#define _LOGMANAGER_H_

#include "Log.h"
#include "StringUtil.h"

#include <vector>
#include <string.h>
#include <stdio.h>

#define	MAX_MESSAGES 8000   
#define MAX_MSGLEN  1024


// pure virtual interface (well, except the destructor which we just leave empty).
class LogListener {
public:
	virtual ~LogListener() {}
	virtual void Log(enum LOG_LEVEL level, const char *msg) = 0;
	virtual const char *getName() const = 0;
};

class FileLogListener : public LogListener {
public:
	FileLogListener(const char *filename);
	~FileLogListener();

	void Log(enum LOG_LEVEL level, const char *msg);

	bool isValid() {
		return (m_logfile != NULL);
	}

	bool isEnable() {
		return m_enable;
	}

	void setEnable(bool enable) {
		m_enable = enable;
	}

	const char *getName() const { return "file"; }

private:
	char *m_filename;
	FILE *m_logfile;
	bool m_enable;
};

class LogContainer {
public:
	LogContainer(const char* shortName, const char* fullName, bool enable = false);
	
	const char *getShortName() const { return m_shortName; }
	const char *getFullName() const { return m_fullName; }

	bool isListener(LogListener *listener) const;
	void addListener(LogListener *listener);
	void removeListener(LogListener *listener);

	void trigger(enum LOG_LEVEL level, const char *msg);

	bool isEnable() const { return m_enable; }
	void setEnable(bool enable) {
		m_enable = enable;
	}

	enum LOG_LEVEL getLevel() const {
		return m_level;
	}

	void setLevel(enum LOG_LEVEL level) {
		m_level = level;
	}

private:
	char m_fullName[128];
	char m_shortName[32];
	bool m_enable;
	enum LOG_LEVEL m_level;

	std::vector<LogListener *> listeners;
};

class ConsoleListener;

// Avoid <windows.h> include through Thread.h
namespace Common {
	class CriticalSection;
}

class LogManager : NonCopyable
{
private:
	LogContainer *m_Log[NUMBER_OF_LOGS];
	Common::CriticalSection *logMutex;
	FileLogListener *m_fileLog;
	ConsoleListener *m_consoleLog;
	static LogManager *m_logManager;  // Singleton. Ugh.

	LogManager();
	~LogManager();
public:

	static u32 GetMaxLevel() { return MAX_LOGLEVEL;	}

	void Log(enum LOG_LEVEL level, enum LOG_TYPE type, 
			 const char *file, int line, const char *fmt, va_list args);

	void setLogLevel(int type, LOG_LEVEL level) {
		m_Log[type]->setLevel(level);
	}

	void setEnable(int type, bool enable) {
		m_Log[type]->setEnable(enable);
	}

	bool isEnable(int type) {
		return m_Log[type]->isEnable();
	}

	const char *getShortName(int type) const {
		return m_Log[type]->getShortName();
	}

	const char *getFullName(int type) const {
		return m_Log[type]->getFullName();
	}

	bool isListener(int type, LogListener *listener) const {
		return m_Log[type]->isListener(listener);
	}

	void addListener(int type, LogListener *listener) {
		m_Log[type]->addListener(listener);
	}

	void removeListener(int type, LogListener *listener);

	FileLogListener *getFileListener() {
		return m_fileLog;
	}

	ConsoleListener *getConsoleListener() {
		return m_consoleLog;
	}

	static LogManager* GetInstance() {
		return m_logManager;
	}

	static void SetInstance(LogManager *logManager) {
		m_logManager = logManager;
	}

	static void Init();
	static void Shutdown();
};

#endif // _LOGMANAGER_H_
