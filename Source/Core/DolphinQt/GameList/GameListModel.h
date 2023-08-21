// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include <QAbstractTableModel>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "Core/TitleDatabase.h"

#include "DolphinQt/GameList/GameTracker.h"

namespace UICommon
{
class GameFile;
}

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
  std::string GetNetPlayName(const UICommon::GameFile& game) const;
  bool ShouldDisplayGameListItem(int index) const;
  void SetSearchTerm(const QString& term);

  // Using a custom sort role as it sometimes differs slightly from the default Qt::DisplayRole.
  static constexpr int SORT_ROLE = Qt::UserRole;

  enum class Column
  {
    Platform = 0,
    Banner,
    Title,
    Description,
    Maker,
    ID,
    Country,
    Size,
    FileName,
    FilePath,
    FileFormat,
    BlockSize,
    Compression,
    Tags,
    Count,
  };

  void AddGame(const std::shared_ptr<const UICommon::GameFile>& game);
  void UpdateGame(const std::shared_ptr<const UICommon::GameFile>& game);
  void RemoveGame(const std::string& path);

  std::shared_ptr<const UICommon::GameFile> FindGame(const std::string& path) const;
  std::shared_ptr<const UICommon::GameFile> FindSecondDisc(const UICommon::GameFile& game) const;

  void SetScale(float scale);
  float GetScale() const;

  const QStringList& GetAllTags() const;
  const QStringList GetGameTags(const std::string& path) const;

  void AddGameTag(const std::string& path, const QString& name);
  void RemoveGameTag(const std::string& path, const QString& name);

  void NewTag(const QString& name);
  void DeleteTag(const QString& name);

  void PurgeCache();

private:
  // Index in m_games, or -1 if it isn't found
  int FindGameIndex(const std::string& path) const;

  QStringList m_tag_list;
  QMap<QString, QVariant> m_game_tags;

  GameTracker m_tracker;
  QList<std::shared_ptr<const UICommon::GameFile>> m_games;
  Core::TitleDatabase m_title_database;
  QString m_term;
  float m_scale = 1.0;
};
