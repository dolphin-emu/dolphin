// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>
#include <wobjectdefs.h>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameListModel.h"

class GameList final : public QStackedWidget
{
  W_OBJECT(GameList)

public:
  explicit GameList(QWidget* parent = nullptr);
  QString GetSelectedGame() const;

  void SetTableView() { SetPreferredView(true); }
  W_SLOT(SetTableView);
  void SetListView() { SetPreferredView(false); }
  W_SLOT(SetListView);
  void SetViewColumn(int col, bool view) { m_table->setColumnHidden(col, !view); }
  W_SLOT(SetViewColumn, (int, bool));
  void OnColumnVisibilityToggled(const QString& row, bool visible);
  W_SLOT(OnColumnVisibilityToggled, (const QString&, bool));

private:
  void ShowContextMenu(const QPoint&);
  W_SLOT(ShowContextMenu, (const QPoint&), W_Access::Private);
  void OpenContainingFolder();
  W_SLOT(OpenContainingFolder, W_Access::Private);
  void OpenProperties();
  W_SLOT(OpenProperties, W_Access::Private);
  void OpenSaveFolder();
  W_SLOT(OpenSaveFolder, W_Access::Private);
  void OpenWiki();
  W_SLOT(OpenWiki, W_Access::Private);
  void SetDefaultISO();
  W_SLOT(SetDefaultISO, W_Access::Private);
  void DeleteFile();
  W_SLOT(DeleteFile, W_Access::Private);
  void InstallWAD();
  W_SLOT(InstallWAD, W_Access::Private);
  void UninstallWAD();
  W_SLOT(UninstallWAD, W_Access::Private);
  void ExportWiiSave();
  W_SLOT(ExportWiiSave, W_Access::Private);
  void CompressISO();
  W_SLOT(CompressISO, W_Access::Private);

public:
  void GameSelected() W_SIGNAL(GameSelected);

private:
  void MakeTableView();
  void MakeListView();
  void MakeEmptyView();
  // We only have two views, just use a bool to distinguish.
  void SetPreferredView(bool table);
  void ConsiderViewChange();

  GameListModel* m_model;
  QSortFilterProxyModel* m_table_proxy;
  QSortFilterProxyModel* m_list_proxy;

  QListView* m_list;
  QTableView* m_table;
  QLabel* m_empty;
  bool m_prefer_table;

protected:
  void keyReleaseEvent(QKeyEvent* event) override;
};
