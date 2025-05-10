// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QStackedWidget;
class QListWidget;

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

private:
  QStackedWidget* m_stacked_panes;
  QListWidget* m_navigation_list;
};

enum class TabIndex
{
  General = 0,
  Audio = 2
};

class SettingsWindow final : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit SettingsWindow(QWidget* parent = nullptr);

  void SelectGeneralPane();
  void SelectAudioPane();
};
