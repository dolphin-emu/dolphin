// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>
#include <QFileSystemModel>
#include <QGroupBox>
#include <QPushButton>
#include <QTableView>
#include <QTreeView>

#include "DolphinQt/Scripting/ScriptsListModel.h"

namespace Core
{
enum class State;
}

class ScriptingWidget : public QDockWidget
{
public:
  ScriptingWidget(QWidget* parent = nullptr);
  void UpdateIcons();
  void AddNewScript();
  void RestartSelectedScripts();
  void ToggleSelectedScripts();
  void ToggleFavorite(const QModelIndex& index);

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void OnEmulationStateChanged(Core::State state);
  void OnDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                     const QList<int>& roles);
  // Re-root the tree when the scripts path changes in settings; ignored while a game is running
  // since that view is pinned to the game-specific subdirectory.
  void RefreshScriptsRoot();

  void OpenScriptsFolder();
  void OnTreeClicked(const QModelIndex& index);

  QPushButton* m_button_add_new;
  QPushButton* m_button_reload_selected;
  QPushButton* m_button_open_folder;
  QGroupBox* m_scripts_group;

  ScriptsFileSystemModel* m_scripts_model;
  QTreeView* m_tree;
  bool m_game_running = false;
};
