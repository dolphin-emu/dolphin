// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <QDialog>
#include <QModelIndexList>

#include "Core/Core.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
class System;
}  // namespace Core
class BranchWatchProxyModel;
class BranchWatchTableModel;
class CodeWidget;
class QAction;
class QMenu;
class QPoint;
class QPushButton;
class QStatusBar;
class QTableView;
class QTimer;
class QToolBar;
class QWidget;

namespace BranchWatchTableModelColumn
{
enum EnumType : int;
}
namespace BranchWatchTableModelUserRole
{
enum EnumType : int;
}

class BranchWatchDialog : public QDialog
{
  Q_OBJECT

  using Column = BranchWatchTableModelColumn::EnumType;
  using UserRole = BranchWatchTableModelUserRole::EnumType;

public:
  explicit BranchWatchDialog(Core::System& system, Core::BranchWatch& branch_watch,
                             CodeWidget* code_widget, QWidget* parent = nullptr);
  void done(int r) override;
  int exec() override;
  void open() override;

private:
  void OnStartPause(bool checked);
  void OnClearBranchWatch();
  void OnSave();
  void OnSaveAs();
  void OnLoad();
  void OnLoadFrom();
  void OnCodePathWasTaken();
  void OnCodePathNotTaken();
  void OnBranchWasOverwritten();
  void OnBranchNotOverwritten();
  void OnWipeRecentHits();
  void OnWipeInspection();
  void OnTimeout();
  void OnEmulationStateChanged(Core::State new_state);
  void OnHelp();
  void OnToggleAutoSave(bool checked);
  void OnHideShowControls(bool checked);
  void OnToggleIgnoreApploader(bool checked);

  void OnTableClicked(const QModelIndex& index);
  void OnTableContextMenu(const QPoint& pos);
  void OnTableHeaderContextMenu(const QPoint& pos);
  void OnTableDelete(QModelIndexList index_list);
  void OnTableDeleteKeypress();
  void OnTableSetBLR(QModelIndexList index_list);
  void OnTableSetNOP(QModelIndexList index_list);
  void OnTableCopyAddress(QModelIndexList index_list);

public:
  // TODO: Step doesn't cause EmulationStateChanged to be emitted, so it has to call this manually.
  void Update();
  // TODO: There seems to be a lack of a ubiquitous signal for when symbols change.
  void UpdateSymbols();

private:
  void UpdateStatus();
  void Save(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void Load(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void AutoSave(const Core::CPUThreadGuard& guard);

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;
  CodeWidget* m_code_widget;

  QPushButton *m_btn_start_pause, *m_btn_clear_watch, *m_btn_path_was_taken, *m_btn_path_not_taken,
      *m_btn_was_overwritten, *m_btn_not_overwritten, *m_btn_wipe_recent_hits;
  QAction* m_act_autosave;
  QMenu* m_mnu_column_visibility;

  QToolBar* m_control_toolbar;
  QTableView* m_table_view;
  BranchWatchProxyModel* m_table_proxy;
  BranchWatchTableModel* m_table_model;
  QStatusBar* m_status_bar;
  QTimer* m_timer;

  std::optional<std::string> m_autosave_filepath;
};
