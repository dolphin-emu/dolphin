// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QIcon>

#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/ToolBar.h"

static QSize ICON_SIZE(32, 32);

ToolBar::ToolBar(QWidget* parent) : QToolBar(parent)
{
  setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  setMovable(false);
  setFloatable(false);
  setIconSize(ICON_SIZE);

  MakeActions();
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &ToolBar::UpdateIcons);
  UpdateIcons();

  EmulationStopped();
}

void ToolBar::EmulationStarted()
{
  m_play_action->setEnabled(false);
  m_play_action->setVisible(false);
  m_pause_action->setEnabled(true);
  m_pause_action->setVisible(true);
  m_stop_action->setEnabled(true);
  m_stop_action->setVisible(true);
  m_fullscreen_action->setEnabled(true);
  m_screenshot_action->setEnabled(true);
}

void ToolBar::EmulationPaused()
{
  m_play_action->setEnabled(true);
  m_play_action->setVisible(true);
  m_pause_action->setEnabled(false);
  m_pause_action->setVisible(false);
  m_stop_action->setEnabled(true);
  m_stop_action->setVisible(true);
}

void ToolBar::EmulationStopped()
{
  m_play_action->setEnabled(true);
  m_play_action->setVisible(true);
  m_pause_action->setEnabled(false);
  m_pause_action->setVisible(false);
  m_stop_action->setEnabled(false);
  m_fullscreen_action->setEnabled(false);
  m_screenshot_action->setEnabled(false);
}

void ToolBar::MakeActions()
{
  constexpr int button_width = 65;
  m_open_action = addAction(tr("Open"), this, &ToolBar::OpenPressed);
  widgetForAction(m_open_action)->setMinimumWidth(button_width);

  m_play_action = addAction(tr("Play"), this, &ToolBar::PlayPressed);
  widgetForAction(m_play_action)->setMinimumWidth(button_width);

  m_pause_action = addAction(tr("Pause"), this, &ToolBar::PausePressed);
  widgetForAction(m_pause_action)->setMinimumWidth(button_width);

  m_stop_action = addAction(tr("Stop"), this, &ToolBar::StopPressed);
  widgetForAction(m_stop_action)->setMinimumWidth(button_width);

  m_fullscreen_action = addAction(tr("FullScr"), this, &ToolBar::FullScreenPressed);
  widgetForAction(m_fullscreen_action)->setMinimumWidth(button_width);

  m_screenshot_action = addAction(tr("ScrShot"), this, &ToolBar::ScreenShotPressed);
  widgetForAction(m_screenshot_action)->setMinimumWidth(button_width);

  addSeparator();

  m_config_action = addAction(tr("Config"), this, &ToolBar::SettingsPressed);
  widgetForAction(m_config_action)->setMinimumWidth(button_width);

  m_graphics_action = addAction(tr("Graphics"), this, &ToolBar::GraphicsPressed);
  widgetForAction(m_graphics_action)->setMinimumWidth(button_width);

  m_controllers_action = addAction(tr("Controllers"), this, &ToolBar::ControllersPressed);
  widgetForAction(m_controllers_action)->setMinimumWidth(button_width);
  m_controllers_action->setEnabled(true);
}

void ToolBar::UpdateIcons()
{
  m_open_action->setIcon(Resources::GetScaledThemeIcon("open"));
  m_play_action->setIcon(Resources::GetScaledThemeIcon("play"));
  m_pause_action->setIcon(Resources::GetScaledThemeIcon("pause"));
  m_stop_action->setIcon(Resources::GetScaledThemeIcon("stop"));
  m_fullscreen_action->setIcon(Resources::GetScaledThemeIcon("fullscreen"));
  m_screenshot_action->setIcon(Resources::GetScaledThemeIcon("screenshot"));
  m_config_action->setIcon(Resources::GetScaledThemeIcon("config"));
  m_controllers_action->setIcon(Resources::GetScaledThemeIcon("classic"));
  m_graphics_action->setIcon(Resources::GetScaledThemeIcon("graphics"));
}
