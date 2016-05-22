// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QMap>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QThread>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameTracker.h"

class GameLoader;

// Watches directories and loads GameFiles in a separate thread.
// To use this, just add directories using AddDirectory, and listen for the
// GameLoaded and GameRemoved signals. Ignore the PathChanged signal, it's
// only there because the Qt people made fileChanged and directoryChanged
// private.
class GameTracker final : public QFileSystemWatcher
{
	Q_OBJECT

public:
	explicit GameTracker(QObject* parent = nullptr);
	~GameTracker();

public slots:
	void AddDirectory(const QString& dir);
	void RemoveDirectory(const QString& dir);

signals:
	void GameLoaded(QSharedPointer<GameFile> game);
	void GameRemoved(const QString& path);

	void PathChanged(const QString& path);

private:
	void UpdateDirectory(const QString& dir);
	void UpdateFile(const QString& path);

	// game path -> number of directories that track it
	QMap<QString, int> m_tracked_files;
	QThread m_loader_thread;
	GameLoader* m_loader;
};

class GameLoader final : public QObject
{
	Q_OBJECT

public slots:
	void LoadGame(const QString& path)
	{
		GameFile* game = new GameFile(path);
		if (game->IsValid())
			emit GameLoaded(QSharedPointer<GameFile>(game));
	}

signals:
	void GameLoaded(QSharedPointer<GameFile> game);
};

Q_DECLARE_METATYPE(QSharedPointer<GameFile>)
