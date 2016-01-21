// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <QObject>
#include <QString>
#include <QTextEdit>
#include <queue>
#include <QWidget>
#include <utility>

#include "Common/Logging/LogManager.h"

class LogViewer final : public QTextEdit, LogListener
{
	Q_OBJECT
public:
	explicit LogViewer(QWidget* parent);
	~LogViewer();
	void Log(LogTypes::LOG_LEVELS, const char* msg) override;
private slots:
	void displayLog();
private:
	std::mutex m_LogEntry;
	std::queue<std::pair<LogTypes::LOG_LEVELS, QString>> msgQueue;
};
