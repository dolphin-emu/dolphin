// Copyright 2021 Dolphin Emulator Project
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
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();

  void OnDelete();
  void OnClear();
  void OnToggleOnOff();
  void OnLoadFromFile();
  void OnSaveToFile();

  void UpdateIcons();
  void UpdateButtonsEnabled();

  QToolBar* m_toolbar;
  QTableWidget* m_table;
  QAction* m_delete;
  QAction* m_clear;
  QAction* m_toggle_on_off;
  QAction* m_load;
  QAction* m_save;
};
