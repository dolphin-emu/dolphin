// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAbstractTableModel>
#include <QString>

#include "Core/TitleDatabase.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameTracker.h"

class GameListModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  explicit GameListModel(QObject* parent = nullptr);

  // Qt's Model/View stuff uses these overrides.
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;

  QSharedPointer<GameFile> GetGameFile(int index) const;
  // Path of the Game at the specified index.
  QString GetPath(int index) const { return m_games[index]->GetFilePath(); }
  // Unique ID of the Game at the specified index
  QString GetUniqueID(int index) const { return m_games[index]->GetUniqueID(); }
  bool ShouldDisplayGameListItem(int index) const;
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

  void UpdateGame(const QSharedPointer<GameFile>& game);
  void RemoveGame(const QString& path);

private:
  // Index in m_games, or -1 if it isn't found
  int FindGame(const QString& path) const;

  GameTracker m_tracker;
  QList<QSharedPointer<GameFile>> m_games;
  Core::TitleDatabase m_title_database;
};
