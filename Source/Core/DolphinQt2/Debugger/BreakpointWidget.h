// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QAction;
class QTableWidget;
class QToolBar;
class QCloseEvent;

class BreakpointWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit BreakpointWidget(QWidget* parent = nullptr);
  ~BreakpointWidget();

  void AddBP(u32 addr);
  void AddAddressMBP(u32 addr, bool on_read = true, bool on_write = true, bool do_log = true,
                     bool do_break = true);
  void AddRangedMBP(u32 from, u32 to, bool do_read = true, bool do_write = true, bool do_log = true,
                    bool do_break = true);
  void Update();

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void CreateWidgets();

  void OnDelete();
  void OnClear();
  void OnNewBreakpoint();
  void OnLoad();
  void OnSave();

  QToolBar* m_toolbar;
  QTableWidget* m_table;
  QAction* m_load;
  QAction* m_save;
};
