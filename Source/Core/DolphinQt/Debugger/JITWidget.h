// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class BreakpointWidget;
class ClickableStatusBar;
class CodeWidget;
namespace Core
{
enum class State;
class System;
}  // namespace Core
struct JitBlock;
class JitBlockProxyModel;
class JitBlockTableModel;
namespace JitBlockTableModelColumn
{
enum EnumType : int;
}
namespace JitBlockTableModelUserRole
{
enum EnumType : int;
}
class MemoryWidget;
class QCloseEvent;
class QFont;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QSplitter;
class QTableView;

class JITWidget final : public QDockWidget
{
  Q_OBJECT

  using Column = JitBlockTableModelColumn::EnumType;
  using UserRole = JitBlockTableModelUserRole::EnumType;

signals:
  void HideSignal();
  void ShowSignal();
  void SetCodeAddress(u32 address);

public:
  explicit JITWidget(Core::System& system, QWidget* parent = nullptr);
  ~JITWidget() override;

  JITWidget(const JITWidget&) = delete;
  JITWidget(JITWidget&&) = delete;
  JITWidget& operator=(const JITWidget&) = delete;
  JITWidget& operator=(JITWidget&&) = delete;

  // Always connected slots (external signals)
  void OnRequestPPCComparison(u32 address, bool translate_address);

private:
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

  void UpdateProfilingButton();
  void UpdateOtherButtons(Core::State state);
  void UpdateDebugFont(const QFont& font);
  void ClearDisassembly();
  void ShowFreeMemoryStatus();
  void UpdateContent(Core::State state);
  void CrossDisassemble(const JitBlock& block);
  void CrossDisassemble(const QModelIndex& index);
  void CrossDisassemble();
  void TableEraseBlocks();

  // Setup and teardown
  void LoadQSettings();
  void SaveQSettings() const;
  void ConnectSlots();
  void DisconnectSlots();
  void Show();
  void Hide();

  // Always connected slots (external signals)
  void OnVisibilityToggled(bool visible);
  void OnDebugModeToggled(bool visible);

  // Always connected slots (internal signals)
  void OnToggleProfiling(bool enabled);
  void OnClearCache();
  void OnWipeProfiling();
  void OnTableCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
  void OnTableDoubleClicked(const QModelIndex& index);
  void OnTableContextMenu(const QPoint& pos);
  void OnTableHeaderContextMenu(const QPoint& pos);
  void OnTableMenuViewCode();
  void OnTableMenuEraseBlocks();
  void OnStatusBarPressed();

  // Conditionally connected slots (external signals)
  void OnJitCacheInvalidation();
  void OnUpdateDisasmDialog();
  void OnPPCSymbolsUpdated();
  void OnPPCBreakpointsChanged();
  void OnConfigChanged();
  void OnDebugFontChanged(const QFont& font);
  void OnEmulationStateChanged(Core::State state);

  Core::System& m_system;

  QLineEdit* m_pm_address_covered_line_edit;
  QPushButton* m_clear_cache_button;
  QPushButton* m_toggle_profiling_button;
  QPushButton* m_wipe_profiling_button;
  QTableView* m_table_view;
  JitBlockProxyModel* m_table_proxy;
  JitBlockTableModel* m_table_model;
  QPlainTextEdit* m_ppc_asm_widget;
  QPlainTextEdit* m_host_near_asm_widget;
  QPlainTextEdit* m_host_far_asm_widget;
  QSplitter* m_table_splitter;
  QSplitter* m_disasm_splitter;
  ClickableStatusBar* m_status_bar;

  QMenu* m_table_context_menu;
  QMenu* m_column_visibility_menu;
};
