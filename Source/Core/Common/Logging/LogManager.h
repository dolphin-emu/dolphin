// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdarg>
#include <fstream>
#include <mutex>
#include <set>
#include <string>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"
#include "Common/Logging/Log.h"

#define MAX_MSGLEN  1024

// pure virtual interface
class LogListener
{
public:
	virtual ~LogListener() {}

	virtual void Log(LogTypes::LOG_LEVELS, const char* msg) = 0;

	enum LISTENER
	{
		FILE_LISTENER = 0,
		CONSOLE_LISTENER,
		LOG_WINDOW_LISTENER,

		NUMBER_OF_LISTENERS // Must be last
	};
};

class FileLogListener : public LogListener
{
public:
	FileLogListener(const std::string& filename);

	void Log(LogTypes::LOG_LEVELS, const char* msg) override;

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

	void AddListener(LogListener::LISTENER id) { m_listener_ids[id] = 1; }
	void RemoveListener(LogListener::LISTENER id) { m_listener_ids[id] = 0; }

	void Trigger(LogTypes::LOG_LEVELS, const char* msg);

	bool IsEnabled() const { return m_enable; }
	void SetEnable(bool enable) { m_enable = enable; }

	LogTypes::LOG_LEVELS GetLevel() const { return m_level; }

	void SetLevel(LogTypes::LOG_LEVELS level) { m_level = level; }

	bool HasListeners() const { return bool(m_listener_ids); }

	typedef class BitSet32::Iterator iterator;
	iterator begin() const { return m_listener_ids.begin(); }
	iterator end() const { return m_listener_ids.end(); }

private:
	std::string m_fullName;
	std::string m_shortName;
	bool m_enable;
	LogTypes::LOG_LEVELS m_level;
	BitSet32 m_listener_ids;
};

class ConsoleListener;

class LogManager : NonCopyable
{
private:
	LogContainer* m_Log[LogTypes::NUMBER_OF_LOGS];
	static LogManager* m_logManager;  // Singleton. Ugh.
	std::array<LogListener*, LogListener::NUMBER_OF_LISTENERS> m_listeners;

	LogManager();
	~LogManager();
public:

	static u32 GetMaxLevel() { return MAX_LOGLEVEL; }

	void Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
			 const char* file, int line, const char* fmt, va_list args);

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

	void RegisterListener(LogListener::LISTENER id, LogListener* listener)
	{
		m_listeners[id] = listener;
	}

	void AddListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id)
	{
		m_Log[type]->AddListener(id);
	}

	void RemoveListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id)
	{
		m_Log[type]->RemoveListener(id);
	}

	static LogManager* GetInstance()
	{
		return m_logManager;
	}

	static void SetInstance(LogManager* logManager)
	{
		m_logManager = logManager;
	}

	static void Init();
	static void Shutdown();
};
