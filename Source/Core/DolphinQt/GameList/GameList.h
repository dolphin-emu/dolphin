// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QStackedWidget>

#include "DolphinQt/GameList/GameListModel.h"

class QAbstractItemView;
class QLabel;
class QListView;
class QSortFilterProxyModel;
class QTableView;

namespace UICommon
{
class GameFile;
}

class GameList final : public QStackedWidget
{
  Q_OBJECT

public:
  explicit GameList(QWidget* parent = nullptr);
  ~GameList();

  std::shared_ptr<const UICommon::GameFile> GetSelectedGame() const;
  QList<std::shared_ptr<const UICommon::GameFile>> GetSelectedGames() const;
  bool HasMultipleSelected() const;
  std::shared_ptr<const UICommon::GameFile> FindGame(const std::string& path) const;
  std::shared_ptr<const UICommon::GameFile> FindSecondDisc(const UICommon::GameFile& game) const;
  std::string GetNetPlayName(const UICommon::GameFile& game) const;

  void SetListView() { SetPreferredView(true); }
  void SetGridView() { SetPreferredView(false); }
  void SetViewColumn(int col, bool view);
  void SetSearchTerm(const QString& term);

  void OnColumnVisibilityToggled(const QString& row, bool visible);
  void OnGameListVisibilityChanged();

  void resizeEvent(QResizeEvent* event) override;

  void PurgeCache();

  const GameListModel& GetGameListModel() const { return m_model; }

signals:
  void GameSelected();
  void OnStartWithRiivolution(const UICommon::GameFile& game);
  void NetPlayHost(const UICommon::GameFile& game);
  void SelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);
  void OpenGeneralSettings();
  void OpenGraphicsSettings();

private:
  void ShowHeaderContextMenu(const QPoint& pos);
  void ShowContextMenu(const QPoint&);
  void OpenContainingFolder();
  void OpenProperties();
  void OpenWiiSaveFolder();
  void OpenGCSaveFolder();
  void OpenWiki();
  void StartWithRiivolution();
  void SetDefaultISO();
  void DeleteFile();
#ifdef _WIN32
  bool AddShortcutToDesktop();
#endif
  void InstallWAD();
  void UninstallWAD();
  void ExportWiiSave();
  void ConvertFile();
  void ChangeDisc();
  void NewTag();
  void DeleteTag();
  void UpdateColumnVisibility();

  void ZoomIn();
  void ZoomOut();

  void OnHeaderViewChanged();
  void OnSectionResized(int index, int, int);

  void MakeListView();
  void MakeGridView();
  void MakeEmptyView();
  // We only have two views, just use a bool to distinguish.
  void SetPreferredView(bool list);
  QAbstractItemView* GetActiveView() const;
  QSortFilterProxyModel* GetActiveProxyModel() const;
  void ConsiderViewChange();
  void UpdateFont();

  GameListModel m_model;
  QSortFilterProxyModel* m_list_proxy;
  QSortFilterProxyModel* m_grid_proxy;

  QListView* m_grid;
  QTableView* m_list;
  QLabel* m_empty;
  bool m_prefer_list;

protected:
  void keyPressEvent(QKeyEvent* event) override;
};
