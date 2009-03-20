// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _LOGMANAGER_H
#define _LOGMANAGER_H 

#include "Log.h"
#include "Thread.h"
#include "StringUtil.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <string.h>
#include <stdio.h>

#define	MAX_MESSAGES 8000   
#define MAX_MSGLEN  512


class Listener {
public:
	Listener(const char *name) : m_name(name) {}
	virtual void Log(LogTypes::LOG_LEVELS, const char *msg) = 0;
	virtual const char *getName() { return m_name; }

private:
	const char *m_name;
};

class FileLogListener : public Listener {
public:
	FileLogListener(const char *filename);
	~FileLogListener();

	void Log(LogTypes::LOG_LEVELS, const char *msg);

	bool isValid() {
		return (m_logfile != NULL);
	}

	bool isEnable() {
		return m_enable;
	}

	void setEnable(bool enable) {
		m_enable = enable;
	}
private:
	char *m_filename;
	FILE *m_logfile;
	bool m_enable;
	
};

class ConsoleListener : public Listener
{
public:
	ConsoleListener();
	~ConsoleListener();

	void Open(int Width = 100, int Height = 100,
		char * Name = "Console");
	void Close();
	bool IsOpen();
	void Log(LogTypes::LOG_LEVELS, const char *text);
	void ClearScreen();

private:
#ifdef _WIN32
	HWND GetHwnd(void);
	HANDLE m_hStdOut;
#endif
};

class LogContainer {
public:

	LogContainer(const char* shortName, const char* fullName,
				 bool enable = false) : m_enable(enable) {
		strncpy(m_fullName, fullName, 128);
		strncpy(m_shortName, shortName, 32);
		m_level = LogTypes::LWARNING;
	}
	
	const char *getShortName() {
		return m_shortName;
	}

	const char *getFullName() {
		return m_fullName;
	}

	bool isListener(Listener *listener);

	void addListener(Listener *listener);

	void removeListener(Listener *listener);

	void trigger(LogTypes::LOG_LEVELS, const char *msg);

	bool isEnable() {
		return m_enable;
	}

	void setEnable(bool enable) {
		m_enable = enable;
	}

	LogTypes::LOG_LEVELS getLevel() {
		return m_level;
	}

	void setLevel(LogTypes::LOG_LEVELS level) {
		m_level = level;
	}

private:
	char m_fullName[128];
	char m_shortName[32];
	bool m_enable;
	LogTypes::LOG_LEVELS m_level;

	std::vector<Listener *> listeners;
};

class LogManager 
{
private:

	LogContainer* m_Log[LogTypes::NUMBER_OF_LOGS];
	Common::CriticalSection* logMutex;
	FileLogListener *m_fileLog;
	ConsoleListener *m_consoleLog;
	static LogManager *m_logManager; // FIXME: find a way without singletone


public:
	static u32 GetMaxLevel() { 
		return LOGLEVEL;
	}

	void Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
			 const char *fmt, ...);

	void setLogLevel(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level){
		m_Log[type]->setLevel(level);
	}

	void setEnable(LogTypes::LOG_TYPE type, bool enable) {
		m_Log[type]->setEnable(enable);
	}

	const char *getShortName(LogTypes::LOG_TYPE type) {
		return m_Log[type]->getShortName();
	}

	const char *getFullName(LogTypes::LOG_TYPE type) {
		return m_Log[type]->getFullName();
	}

	bool isListener(LogTypes::LOG_TYPE type, Listener *listener) {
		return m_Log[type]->isListener(listener);
	}

	void addListener(LogTypes::LOG_TYPE type, Listener *listener) {
		m_Log[type]->addListener(listener);
	}

	void removeListener(LogTypes::LOG_TYPE type, Listener *listener);

	FileLogListener *getFileListener() {
		return m_fileLog;
	}

	ConsoleListener *getConsoleListener() {
		return m_consoleLog;
	}

	static LogManager* GetInstance() {
		if (! m_logManager) 
			m_logManager = new LogManager();

		return m_logManager;
	}

	static void SetInstance(LogManager *logManager) {
		m_logManager = logManager;
	}

	LogManager();
	~LogManager();
};

#endif // LOGMANAGER_H
