// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <QDialog>

#include "Core/Core.h"
#include "DolphinQt/Debugger/CodeWidget.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
class System;
}  // namespace Core
class BranchWatchProxyModel;
class BranchWatchTableModel;
class BranchWatchTableView;
class QAction;
class QPushButton;
class QStatusBar;
class QTimer;
class QToolBar;
class QWidget;

class BranchWatchDialog : public QDialog
{
  Q_OBJECT

  // TODO: There seems to be a lack of a ubiquitous signal for when symbols change.
  // This is the best location to catch the signals from MenuBar and CodeViewWidget.
  friend void CodeWidget::UpdateSymbols();
  // TODO: Step doesn't cause EmulationStateChanged to be emitted, so it has to call this manually.
  friend void CodeWidget::Step();

public:
  explicit BranchWatchDialog(Core::System& system, Core::BranchWatch& branch_watch,
                             CodeWidget* code_widget, QWidget* parent = nullptr);
  void done(int r) override;
  int exec() override;
  void open() override;

private:
  [[nodiscard]] QLayout* CreateLayout();

  void OnStartPause(bool checked);
  void OnClearWatch();
  void OnSave();
  void OnSaveAs();
  void OnLoad();
  void OnLoadFrom();
  void OnBranchHasExecuted();
  void OnBranchNotExecuted();
  void OnBranchWasOverwritten();
  void OnBranchNotOverwritten();
  void OnWipeRecentHits();
  void OnTimeout();
  void OnEmulationStateChanged(Core::State new_state);
  void OnHelp();
  void OnToggleAutoSave(bool checked);
  void OnHideShowControls(bool checked);
  void OnToggleDestinationSymbols(bool checked);

  void Update();
  void UpdateSymbols();
  void UpdateStatus();
  void Save(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void Load(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void AutoSave(const Core::CPUThreadGuard& guard);

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;

  CodeWidget* m_code_widget;

  QPushButton *m_btn_start_pause, *m_btn_clear_watch, *m_btn_has_executed, *m_btn_not_executed,
      *m_btn_was_overwritten, *m_btn_not_overwritten, *m_btn_wipe_recent_hits;
  QAction* m_act_autosave;

  QToolBar* m_control_toolbar;
  BranchWatchTableView* m_table_view;
  BranchWatchProxyModel* m_table_proxy;
  BranchWatchTableModel* m_table_model;
  QStatusBar* m_status_bar;
  QTimer* m_timer;

  std::optional<std::string> m_autosave_filepath;
};
