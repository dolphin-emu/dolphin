// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QDirIterator>
#include <QFile>

#include "DolphinQt2/Settings.h"
#include "DolphinQt2/GameList/GameTracker.h"

static const QStringList game_filters{
	QStringLiteral("*.gcm"),
	QStringLiteral("*.iso"),
	QStringLiteral("*.ciso"),
	QStringLiteral("*.gcz"),
	QStringLiteral("*.wbfs"),
	QStringLiteral("*.wad"),
	QStringLiteral("*.elf"),
	QStringLiteral("*.dol")
};

GameTracker::GameTracker(QObject* parent)
	: QFileSystemWatcher(parent)
{
	m_loader = new GameLoader;
	m_loader->moveToThread(&m_loader_thread);

	qRegisterMetaType<QSharedPointer<GameFile>>();
	connect(&m_loader_thread, &QThread::finished, m_loader, &QObject::deleteLater);
	connect(this, &QFileSystemWatcher::directoryChanged, this, &GameTracker::UpdateDirectory);
	connect(this, &QFileSystemWatcher::fileChanged, this, &GameTracker::UpdateFile);
	connect(this, &GameTracker::PathChanged, m_loader, &GameLoader::LoadGame);
	connect(m_loader, &GameLoader::GameLoaded, this, &GameTracker::GameLoaded);

	m_loader_thread.start();

	for (QString dir : Settings().GetPaths())
		AddDirectory(dir);
}

GameTracker::~GameTracker()
{
	m_loader_thread.quit();
	m_loader_thread.wait();
}

void GameTracker::AddDirectory(QString dir)
{
	addPath(dir);
	UpdateDirectory(dir);
}

void GameTracker::UpdateDirectory(const QString& dir)
{
	QDirIterator it(dir, game_filters);
	while (it.hasNext())
	{
		QString path = QFileInfo(it.next()).canonicalFilePath();
		if (!m_tracked_files.contains(path))
		{
			addPath(path);
			m_tracked_files.insert(path);
			emit PathChanged(path);
		}
	}
}

void GameTracker::UpdateFile(const QString& file)
{
	if (QFileInfo(file).exists())
	{
		emit PathChanged(file);
	}
	else if (removePath(file))
	{
		m_tracked_files.remove(file);
		emit GameRemoved(file);
	}
}
