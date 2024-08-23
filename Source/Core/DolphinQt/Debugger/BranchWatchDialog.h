// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <QDialog>
#include <QIcon>
#include <QModelIndexList>

#include "Common/CommonTypes.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
enum class State;
class System;
}  // namespace Core
class PPCSymbolDB;

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
                             PPCSymbolDB& ppc_symbol_db, CodeWidget* code_widget,
                             QWidget* parent = nullptr);
  ~BranchWatchDialog() override;

  BranchWatchDialog(const BranchWatchDialog&) = delete;
  BranchWatchDialog(BranchWatchDialog&&) = delete;
  BranchWatchDialog& operator=(const BranchWatchDialog&) = delete;
  BranchWatchDialog& operator=(BranchWatchDialog&&) = delete;

protected:
  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

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
  void OnThemeChanged();
  void OnHelp();
  void OnToggleAutoSave(bool checked);
  void OnHideShowControls(bool checked);
  void OnToggleIgnoreApploader(bool checked);

  void OnTableClicked(const QModelIndex& index);
  void OnTableContextMenu(const QPoint& pos);
  void OnTableHeaderContextMenu(const QPoint& pos);
  void OnTableDelete();
  void OnTableDeleteKeypress();
  void OnTableSetBLR();
  void OnTableSetNOP();
  void OnTableCopyAddress();
  void OnTableSetBreakpointBreak();
  void OnTableSetBreakpointLog();
  void OnTableSetBreakpointBoth();

  void SaveSettings();

public:
  // TODO: Step doesn't cause EmulationStateChanged to be emitted, so it has to call this manually.
  void Update();

private:
  void UpdateStatus();
  void UpdateIcons();
  void Save(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void Load(const Core::CPUThreadGuard& guard, const std::string& filepath);
  void AutoSave(const Core::CPUThreadGuard& guard);
  void SetStubPatches(u32 value) const;
  void SetBreakpoints(bool break_on_hit, bool log_on_hit) const;

  [[nodiscard]] QMenu* GetTableContextMenu(const QModelIndex& index);

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;
  CodeWidget* m_code_widget;

  QPushButton *m_btn_start_pause, *m_btn_clear_watch, *m_btn_path_was_taken, *m_btn_path_not_taken,
      *m_btn_was_overwritten, *m_btn_not_overwritten, *m_btn_wipe_recent_hits;
  QAction* m_act_autosave;
  QAction* m_act_insert_nop;
  QAction* m_act_insert_blr;
  QAction* m_act_copy_address;
  QMenu* m_mnu_set_breakpoint;
  QAction* m_act_break_on_hit;
  QAction* m_act_log_on_hit;
  QAction* m_act_both_on_hit;
  QMenu* m_mnu_table_context = nullptr;
  QMenu* m_mnu_column_visibility;

  QToolBar* m_control_toolbar;
  QTableView* m_table_view;
  BranchWatchProxyModel* m_table_proxy;
  BranchWatchTableModel* m_table_model;
  QStatusBar* m_status_bar;
  QTimer* m_timer;

  QIcon m_icn_full, m_icn_partial;

  QModelIndexList m_index_list_temp;
  std::optional<std::string> m_autosave_filepath;
};
