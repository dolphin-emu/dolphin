// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>
#include <QScrollBar>
#include <QString>
#include <QTextEdit>
#include <QTimer>
#include <queue>
#include <QWidget>
#include <utility>

#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "DolphinQt2/LogViewer.h"

LogViewer::LogViewer(QWidget* parent)
	: QTextEdit(parent)
{
	setStyleSheet(QString::fromUtf8("QTextEdit{background-color: rgb(21, 21, 21)}"));
	setReadOnly(true);

	LogManager* m_LogManager = LogManager::GetInstance();
	m_LogManager->RegisterListener(LogListener::LOG_WINDOW_LISTENER, this);
	m_LogManager->OpenWindow();

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(displayLog()));
	timer->start(200);
}

LogViewer::~LogViewer()
{
	LogManager* m_LogManager = LogManager::GetInstance();
	if (m_LogManager)
		m_LogManager->CloseWindow();
}

void LogViewer::Log(LogTypes::LOG_LEVELS level, const char* msg)
{
	std::lock_guard<std::mutex> lk(m_LogEntry);
	QString qmsg = QString::fromUtf8(msg).trimmed();
	msgQueue.emplace(std::make_pair(level, qmsg));
}

void LogViewer::displayLog()
{
	std::lock_guard<std::mutex> lk(m_LogEntry);
	bool scroll = false;
	// This function runs on the main gui thread, and needs to finish in a finite time otherwise
	// the GUI will lock up, which could be an issue if new messages are flooding in faster than
	// this function can render them to the screen.
	// So we limit the number of times run via this for loop.
	for (int i = 0; i < 100; i++)
	{
		if (msgQueue.empty())
			break;
		LogTypes::LOG_LEVELS level = msgQueue.front().first;
		QString qmsg = msgQueue.front().second;
		msgQueue.pop();

		//display log time
		setTextColor(QColor(221, 221, 221)); //white
		append(qmsg.left(9));

		//display log message
		switch (level)
		{//grab from .Xdefaults
		case LogTypes::LOG_LEVELS::LERROR:
			setTextColor(QColor(232, 79, 79)); //red
			break;
		case LogTypes::LOG_LEVELS::LWARNING:
			setTextColor(QColor(225, 170, 93)); //yellow
			break;
		case LogTypes::LOG_LEVELS::LNOTICE:
			setTextColor(QColor(184, 214, 140)); //green
			break;
		case LogTypes::LOG_LEVELS::LINFO:
			setTextColor(QColor(109, 135, 189)); //cyan
			break;
		case LogTypes::LOG_LEVELS::LDEBUG:
			setTextColor(QColor(70, 70, 70, 70)); //grey
			break;
		default:
			setTextColor(QColor(221, 221, 221)); //white
			break;
		}
		insertPlainText(qmsg.right(qmsg.length()-9));
		scroll = true;
	}
	if (scroll)
		this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
}
