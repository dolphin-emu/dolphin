// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QAction;
class QTableWidget;
class QTableWidgetItem;
class QToolBar;
class QCloseEvent;

class WatchWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit WatchWidget(QWidget* parent = nullptr);
  ~WatchWidget();

  void AddWatch(QString name, u32 addr);
signals:
  void RequestMemoryBreakpoint(u32 addr);

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnLoad();
  void OnSave();

  void Update();

  void ShowContextMenu();
  void OnItemChanged(QTableWidgetItem* item);
  void DeleteWatch(int row);
  void AddWatchBreakpoint(int row);

  QAction* m_load;
  QAction* m_save;
  QToolBar* m_toolbar;
  QTableWidget* m_table;

  bool m_updating = false;
};
