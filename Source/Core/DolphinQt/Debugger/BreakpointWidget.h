// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QDockWidget>
#include <QString>

#include "Common/CommonTypes.h"

class QAction;
class QCloseEvent;
class QPoint;
class QShowEvent;
class QTableWidget;
class QTableWidgetItem;
class QToolBar;
class QWidget;

namespace Core
{
class System;
}

class CustomDelegate;

class BreakpointWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit BreakpointWidget(QWidget* parent = nullptr);
  ~BreakpointWidget();

  void AddBP(u32 addr);
  void AddBP(u32 addr, bool temp, bool break_on_hit, bool log_on_hit, const QString& condition);
  void AddAddressMBP(u32 addr, bool on_read = true, bool on_write = true, bool do_log = true,
                     bool do_break = true, const QString& condition = {});
  void AddRangedMBP(u32 from, u32 to, bool do_read = true, bool do_write = true, bool do_log = true,
                    bool do_break = true, const QString& condition = {});
  void UpdateButtonsEnabled();
  void Update();

signals:
  void BreakpointsChanged();
  void ShowCode(u32 address);
  void ShowMemory(u32 address);

protected:
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();

  void EditBreakpoint(u32 address, int edit, std::optional<QString> = std::nullopt);
  void EditMBP(u32 address, int edit, std::optional<QString> = std::nullopt);

  void OnClear();
  void OnClicked(QTableWidgetItem* item);
  void OnNewBreakpoint();
  void OnEditBreakpoint(u32 address, bool is_instruction_bp);
  void OnLoad();
  void OnSave();
  void OnContextMenu(const QPoint& pos);
  void OnItemChanged(QTableWidgetItem* item);
  void UpdateIcons();

  Core::System& m_system;

  QToolBar* m_toolbar;
  QTableWidget* m_table;
  QAction* m_new;
  QAction* m_clear;
  QAction* m_load;
  QAction* m_save;
};
