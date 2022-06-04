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

  void ShowContextMenu();
  void OnItemChanged(QTableWidgetItem* item);
  void DeleteWatch(int row);
  void AddWatchBreakpoint(int row);
  void ShowInMemory(int row);
  void UpdateIcons();

  QAction* m_new;
  QAction* m_delete;
  QAction* m_clear;
  QAction* m_load;
  QAction* m_save;
  QToolBar* m_toolbar;
  QTableWidget* m_table;

  bool m_updating = false;

  static constexpr int NUM_COLUMNS = 6;
};
