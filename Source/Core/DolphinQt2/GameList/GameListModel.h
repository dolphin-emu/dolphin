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
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	int rowCount(const QModelIndex& parent) const;
	int columnCount(const QModelIndex& parent) const;

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
	void RemoveGame(QString path);

signals:
	void DirectoryAdded(QString dir);

private:
	GameTracker m_tracker;
	QList<QSharedPointer<GameFile>> m_games;
	// Path -> index in m_games
	QMap<QString, int> m_entries;
};
