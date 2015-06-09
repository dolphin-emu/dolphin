// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdarg>
#include <fstream>
#include <mutex>
#include <set>
#include <string>

#include "Common/Common.h"

#define MAX_MESSAGES 8000
#define MAX_MSGLEN  1024


// pure virtual interface
class LogListener
{
public:
	virtual ~LogListener() {}

	virtual void Log(LogTypes::LOG_LEVELS, const char *msg) = 0;
};

class FileLogListener : public LogListener
{
public:
	FileLogListener(const std::string& filename);

	void Log(LogTypes::LOG_LEVELS, const char *msg) override;

	bool IsValid() const { return m_logfile.good(); }
	bool IsEnabled() const { return m_enable; }
	void SetEnable(bool enable) { m_enable = enable; }

	const char* GetName() const { return "file"; }

private:
	std::mutex m_log_lock;
	std::ofstream m_logfile;
	bool m_enable;
};

class LogContainer
{
public:
	LogContainer(const std::string& shortName, const std::string& fullName, bool enable = false);

	std::string GetShortName() const { return m_shortName; }
	std::string GetFullName() const { return m_fullName; }

	void AddListener(LogListener* listener);
	void RemoveListener(LogListener* listener);

	void Trigger(LogTypes::LOG_LEVELS, const char *msg);

	bool IsEnabled() const { return m_enable; }
	void SetEnable(bool enable) { m_enable = enable; }

	LogTypes::LOG_LEVELS GetLevel() const { return m_level; }

	void SetLevel(LogTypes::LOG_LEVELS level) { m_level = level; }

	bool HasListeners() const { return !m_listeners.empty(); }

private:
	std::string m_fullName;
	std::string m_shortName;
	bool m_enable;
	LogTypes::LOG_LEVELS m_level;
	std::mutex m_listeners_lock;
	std::set<LogListener*> m_listeners;
};

class ConsoleListener;

class LogManager : NonCopyable
{
private:
	LogContainer* m_Log[LogTypes::NUMBER_OF_LOGS];
	FileLogListener *m_fileLog;
	ConsoleListener *m_consoleLog;
	static LogManager *m_logManager;  // Singleton. Ugh.

	LogManager();
	~LogManager();
public:

	static u32 GetMaxLevel() { return MAX_LOGLEVEL; }

	void Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
			 const char *file, int line, const char *fmt, va_list args);

	void SetLogLevel(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level)
	{
		m_Log[type]->SetLevel(level);
	}

	void SetEnable(LogTypes::LOG_TYPE type, bool enable)
	{
		m_Log[type]->SetEnable(enable);
	}

	bool IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level = LogTypes::LNOTICE) const
	{
		return m_Log[type]->IsEnabled() && m_Log[type]->GetLevel() >= level;
	}

	std::string GetShortName(LogTypes::LOG_TYPE type) const
	{
		return m_Log[type]->GetShortName();
	}

	std::string GetFullName(LogTypes::LOG_TYPE type) const
	{
		return m_Log[type]->GetFullName();
	}

	void AddListener(LogTypes::LOG_TYPE type, LogListener *listener)
	{
		m_Log[type]->AddListener(listener);
	}

	void RemoveListener(LogTypes::LOG_TYPE type, LogListener *listener)
	{
		m_Log[type]->RemoveListener(listener);
	}

	FileLogListener *GetFileListener() const
	{
		return m_fileLog;
	}

	ConsoleListener *GetConsoleListener() const
	{
		return m_consoleLog;
	}

	static LogManager* GetInstance()
	{
		return m_logManager;
	}

	static void SetInstance(LogManager *logManager)
	{
		m_logManager = logManager;
	}

	static void Init();
	static void Shutdown();
};
