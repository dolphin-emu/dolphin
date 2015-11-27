// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QThread>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameTracker.h"

class GameLoader;

// Watches directories and loads GameFiles in a separate thread.
// To use this, just add directories using AddDirectory, and listen for the
// GameLoaded and GameRemoved signals.
class GameTracker final : public QFileSystemWatcher
{
	Q_OBJECT

public:
	explicit GameTracker(QObject* parent = nullptr);
	~GameTracker();

public slots:
	void AddDirectory(QString dir);

signals:
	void GameLoaded(QSharedPointer<GameFile> game);
	void GameRemoved(QString path);

	void PathChanged(QString path);

private:
	void UpdateDirectory(QString dir);
	void UpdateFile(QString path);
	void GenerateFilters();

	QSet<QString> m_tracked_files;
	QStringList m_filters;
	QThread m_loader_thread;
	GameLoader* m_loader;
};

class GameLoader : public QObject
{
	Q_OBJECT

public slots:
	void LoadGame(QString path)
	{
		GameFile* game = new GameFile(path);
		if (game->IsValid())
			emit GameLoaded(QSharedPointer<GameFile>(game));
	}

signals:
	void GameLoaded(QSharedPointer<GameFile> game);
};

Q_DECLARE_METATYPE(QSharedPointer<GameFile>)
