// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameListModel.h"

class GameList final : public QStackedWidget
{
  Q_OBJECT

public:
  explicit GameList(QWidget* parent = nullptr);
  QSharedPointer<GameFile> GetSelectedGame() const;

  void SetListView() { SetPreferredView(true); }
  void SetGridView() { SetPreferredView(false); }
  void SetViewColumn(int col, bool view) { m_list->setColumnHidden(col, !view); }
  void OnColumnVisibilityToggled(const QString& row, bool visible);
  void OnGameListVisibilityChanged();

signals:
  void GameSelected();
  void EmulationStarted();
  void EmulationStopped();
  void NetPlayHost(const QString& game_id);

private:
  void ShowContextMenu(const QPoint&);
  void OpenContainingFolder();
  void OpenProperties();
  void OpenSaveFolder();
  void OpenWiki();
  void SetDefaultISO();
  void DeleteFile();
  void InstallWAD();
  void UninstallWAD();
  void ExportWiiSave();
  void CompressISO();
  void OnHeaderViewChanged();

  void MakeListView();
  void MakeGridView();
  void MakeEmptyView();
  // We only have two views, just use a bool to distinguish.
  void SetPreferredView(bool list);
  void ConsiderViewChange();

  GameListModel* m_model;
  QSortFilterProxyModel* m_list_proxy;
  QSortFilterProxyModel* m_grid_proxy;

  QListView* m_grid;
  QTableView* m_list;
  QLabel* m_empty;
  bool m_prefer_list;

protected:
  void keyReleaseEvent(QKeyEvent* event) override;
};
