// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <QAbstractTableModel>
#include <QString>

#include "DolphinQt2/GameList/GameTracker.h"
#include "UICommon/GameFile.h"

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

  std::shared_ptr<const UICommon::GameFile> GetGameFile(int index) const;
  // Path of the game at the specified index.
  QString GetPath(int index) const { return QString::fromStdString(m_games[index]->GetFilePath()); }
  // Unique identifier of the game at the specified index.
  QString GetUniqueIdentifier(int index) const
  {
    return QString::fromStdString(m_games[index]->GetUniqueIdentifier());
  }
  bool ShouldDisplayGameListItem(int index) const;
  void SetSearchTerm(const QString& term);

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
    COL_FILE_NAME,
    NUM_COLS
  };

  void UpdateGame(const std::shared_ptr<const UICommon::GameFile>& game);
  void RemoveGame(const std::string& path);

private:
  // Index in m_games, or -1 if it isn't found
  int FindGame(const std::string& path) const;

  GameTracker m_tracker;
  QList<std::shared_ptr<const UICommon::GameFile>> m_games;
  QString m_term;
};
