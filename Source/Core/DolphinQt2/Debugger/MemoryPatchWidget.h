// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

class QAction;
class QTableWidget;
class QToolBar;
class QCloseEvent;

class MemoryPatchWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit MemoryPatchWidget(QWidget* parent = nullptr);
  ~MemoryPatchWidget();

  void Update();

signals:
  void MemoryPatchesChanged();

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void CreateWidgets();

  void OnDelete();
  void OnClear();
  void OnToggleOnOff();

  void UpdateIcons();

  QToolBar* m_toolbar;
  QTableWidget* m_table;
  QAction* m_delete;
  QAction* m_clear;
  QAction* m_toggle_on_off;
};
