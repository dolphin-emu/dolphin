// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAbstractTableModel>
#include <QString>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameTracker.h"

class GameListModel final : public QAbstractTableModel
{
	Q_OBJECT

public:
	explicit GameListModel(QObject* parent = nullptr);

	// Qt's Model/View stuff uses these overrides.
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex& parent) const override;
	int columnCount(const QModelIndex& parent) const override;

	// Path of the Game at the specified index.
	QString GetPath(int index) const { return m_games[index]->GetPath(); }

	enum
	{
		COL_PLATFORM = 0,
		COL_ID,
		COL_BANNER,
		COL_TITLE,
		COL_DESCRIPTION,
		COL_MAKER,
		COL_SIZE,
		COL_COUNTRY,
		COL_RATING,
		NUM_COLS
	};

public slots:
	void UpdateGame(QSharedPointer<GameFile> game);
	void RemoveGame(const QString& path);

signals:
	void DirectoryAdded(const QString& dir);
	void DirectoryRemoved(const QString& dir);

private:
	// Index in m_games, or -1 if it isn't found
	int FindGame(const QString& path) const;

	GameTracker m_tracker;
	QList<QSharedPointer<GameFile>> m_games;
};
