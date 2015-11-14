// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>

#include "Core/ConfigManager.h"
#include "DolphinQt/GameList/GameTracker.h"

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

	GenerateFilters();

	m_loader_thread.start();

	for (const std::string& dir : SConfig::GetInstance().m_ISOFolder)
		AddDirectory(QString::fromStdString(dir));
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

void GameTracker::UpdateDirectory(QString dir)
{
	QDirIterator it(dir, m_filters);
	while (it.hasNext())
	{
		QString path = QFileInfo(it.next()).canonicalFilePath();
		if (!m_tracked_files.contains(path))
		{
			if (addPath(path))
			{
				m_tracked_files.insert(path);
				emit PathChanged(path);
			}
		}
	}
}

void GameTracker::UpdateFile(QString file)
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

void GameTracker::GenerateFilters()
{
	m_filters.clear();
	if (SConfig::GetInstance().m_ListGC)
		m_filters << tr("*.gcm");
	if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
		m_filters << tr("*.iso") << tr("*.ciso") << tr("*.gcz") << tr("*.wbfs");
	if (SConfig::GetInstance().m_ListWad)
		m_filters << tr("*.wad");
	if (SConfig::GetInstance().m_ListElfDol)
		m_filters << tr("*.elf") << tr("*.dol");
}
