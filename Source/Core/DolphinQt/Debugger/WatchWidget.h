// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QAction;
class QCloseEvent;
class QShowEvent;
class QTableWidget;
class QTableWidgetItem;
class QToolBar;

namespace Core
{
class CPUThreadGuard;
class System;
};  // namespace Core

class WatchWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit WatchWidget(QWidget* parent = nullptr);
  ~WatchWidget();

  void AddWatch(QString name, u32 addr);
signals:
  void RequestMemoryBreakpoint(u32 addr);
  void ShowMemory(u32 addr);

protected:
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnDelete();
  void OnClear();
  void OnNewWatch();

  void OnLoad();
  void OnSave();

  void UpdateButtonsEnabled();
  void Update();
  void SetEmptyRow(int row);

  void ShowContextMenu();
  void OnItemChanged(QTableWidgetItem* item);
  void LockWatchAddress(const Core::CPUThreadGuard& guard, u32 address);
  void DeleteSelectedWatches();
  void DeleteWatch(const Core::CPUThreadGuard& guard, int row);
  void DeleteWatchAndUpdate(int row);
  void AddWatchBreakpoint(int row);
  void ShowInMemory(int row);
  void UpdateIcons();
  void LockSelectedWatches();
  void UnlockSelectedWatches();

  Core::System& m_system;

  QAction* m_new;
  QAction* m_delete;
  QAction* m_clear;
  QAction* m_load;
  QAction* m_save;
  QToolBar* m_toolbar;
  QTableWidget* m_table;

  bool m_updating = false;

  static constexpr int NUM_COLUMNS = 7;
  static constexpr int COLUMN_INDEX_LABEL = 0;
  static constexpr int COLUMN_INDEX_ADDRESS = 1;
  static constexpr int COLUMN_INDEX_HEX = 2;
  static constexpr int COLUMN_INDEX_DECIMAL = 3;
  static constexpr int COLUMN_INDEX_STRING = 4;
  static constexpr int COLUMN_INDEX_FLOAT = 5;
  static constexpr int COLUMN_INDEX_LOCK = 6;
};
