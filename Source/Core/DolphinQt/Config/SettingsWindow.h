// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QStackedWidget;
class QListWidget;
class MainWindow;
class QEvent;

// A settings window with a QListWidget to switch between panes of a QStackedWidget.
class StackedSettingsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit StackedSettingsWindow(QWidget* parent = nullptr);

  void ActivatePane(int index);

protected:
  void AddPane(QWidget*, const QString& name);

  // Adds a scrollable Pane.
  void AddWrappedPane(QWidget*, const QString& name);

  // For derived classes to call after they create their settings panes.
  void OnDoneCreatingPanes();

  void changeEvent(QEvent* event) override;

private:
  void UpdateNavigationListStyle();

  QStackedWidget* m_stacked_panes = nullptr;
  QListWidget* m_navigation_list = nullptr;
  bool m_handling_theme_change = false;
};

enum class SettingsWindowPaneIndex : int
{
  General = 0,
  Graphics,
  Controllers,
  Interface,
  OnScreenDisplay,
  Audio,
  Paths,
  GameCube,
  Wii,
  Advanced,
};

class SettingsWindow final : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit SettingsWindow(MainWindow* parent);

  void SelectPane(SettingsWindowPaneIndex);
};
