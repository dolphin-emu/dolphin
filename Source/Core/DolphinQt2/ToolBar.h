// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAction>
#include <QLineEdit>
#include <QToolBar>
#include <wobjectdefs.h>

class ToolBar final : public QToolBar
{
  W_OBJECT(ToolBar)

public:
  explicit ToolBar(QWidget* parent = nullptr);

public:
  void EmulationStarted();
  W_SLOT(EmulationStarted);
  void EmulationPaused();
  W_SLOT(EmulationPaused);
  void EmulationStopped();
  W_SLOT(EmulationStopped);

  void OpenPressed() W_SIGNAL(OpenPressed);
  void PlayPressed() W_SIGNAL(PlayPressed);
  void PausePressed() W_SIGNAL(PausePressed);
  void StopPressed() W_SIGNAL(StopPressed);
  void FullScreenPressed() W_SIGNAL(FullScreenPressed);
  void ScreenShotPressed() W_SIGNAL(ScreenShotPressed);

  void SettingsPressed() W_SIGNAL(SettingsPressed);
  void ControllersPressed() W_SIGNAL(ControllersPressed);
  void GraphicsPressed() W_SIGNAL(GraphicsPressed);

private:
  void MakeActions();
  void UpdateIcons();

  QAction* m_open_action;
  QAction* m_play_action;
  QAction* m_pause_action;
  QAction* m_stop_action;
  QAction* m_fullscreen_action;
  QAction* m_screenshot_action;
  QAction* m_config_action;
  QAction* m_controllers_action;
  QAction* m_graphics_action;
};
