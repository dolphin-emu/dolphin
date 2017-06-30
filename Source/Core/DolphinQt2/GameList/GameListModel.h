// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <wobjectdefs.h>

#include "Core/TitleDatabase.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameTracker.h"

class GameListModel final : public QAbstractTableModel
{
  W_OBJECT(GameListModel)

public:
  explicit GameListModel(QObject* parent = nullptr);

  // Qt's Model/View stuff uses these overrides.
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;

  // Path of the Game at the specified index.
  QString GetPath(int index) const { return m_games[index]->GetFilePath(); }
  enum
  {
    COL_PLATFORM = 0,
    COL_BANNER,
    COL_TITLE,
    COL_DESCRIPTION,
    COL_MAKER,
    COL_ID,
    COL_COUNTRY,
    COL_SIZE,
    COL_RATING,
    NUM_COLS
  };

public:
  void UpdateGame(QSharedPointer<GameFile> game);
  W_SLOT(UpdateGame, (QSharedPointer<GameFile>));
  void RemoveGame(const QString& path);
  W_SLOT(RemoveGame, (const QString&));

  void DirectoryAdded(const QString& dir) W_SIGNAL(DirectoryAdded, (const QString&), dir);
  void DirectoryRemoved(const QString& dir) W_SIGNAL(DirectoryRemoved, (const QString&), dir);

private:
  // Index in m_games, or -1 if it isn't found
  int FindGame(const QString& path) const;

  GameTracker m_tracker;
  QList<QSharedPointer<GameFile>> m_games;
  Core::TitleDatabase m_title_database;
};
