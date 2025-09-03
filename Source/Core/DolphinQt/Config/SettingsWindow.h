// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

namespace QtUtils
{
class VerticalTabsTabWidget;
}
class MainWindow;

// A settings window that contains a tab widget with vertical tabs.
class TabbedSettingsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit TabbedSettingsWindow(QWidget* parent = nullptr);

  void ActivatePane(int index);

protected:
  void AddPane(QWidget*, const QString& name);

  // Adds a scrollable Pane.
  void AddWrappedPane(QWidget*, const QString& name);

  // For derived classes to call after they create their settings panes.
  void OnDoneCreatingPanes();

private:
  QtUtils::VerticalTabsTabWidget* m_tabbed_panes{};
};

enum class SettingsWindowPaneIndex : int
{
  General = 0,
  Graphics,
  Controllers,
  Interface,
  Audio,
  Paths,
  GameCube,
  Wii,
  Advanced,
};

class SettingsWindow final : public TabbedSettingsWindow
{
  Q_OBJECT
public:
  explicit SettingsWindow(MainWindow* parent);

  void SelectPane(SettingsWindowPaneIndex);
};
