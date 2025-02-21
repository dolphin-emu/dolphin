// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigRadioInt;
class ConfigStringChoice;
class QLabel;
class QVBoxLayout;
class ToolTipCheckBox;
class ToolTipComboBox;

class InterfacePane final : public QWidget
{
  Q_OBJECT
public:
  explicit InterfacePane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void CreateUI();
  void CreateInGame();
  void AddDescriptions();
  void ConnectLayout();
  void UpdateShowDebuggingCheckbox();
  void LoadUserStyle();
  void OnUserStyleChanged();
  void OnLanguageChanged();

  QVBoxLayout* m_main_layout;
  ConfigStringChoice* m_combobox_language;

  ConfigStringChoice* m_combobox_theme;
  ToolTipComboBox* m_combobox_userstyle;
  QLabel* m_label_userstyle;
  ConfigBool* m_checkbox_top_window;
  ConfigBool* m_checkbox_use_builtin_title_database;
  ToolTipCheckBox* m_checkbox_show_debugging_ui;
  ConfigBool* m_checkbox_focused_hotkeys;
  ConfigBool* m_checkbox_use_covers;
  ConfigBool* m_checkbox_disable_screensaver;

  ConfigBool* m_checkbox_confirm_on_stop;
  ConfigBool* m_checkbox_use_panic_handlers;
  ConfigBool* m_checkbox_enable_osd;
  ConfigBool* m_checkbox_show_active_title;
  ConfigBool* m_checkbox_pause_on_focus_lost;
  ConfigRadioInt* m_radio_cursor_visible_movement;
  ConfigRadioInt* m_radio_cursor_visible_never;
  ConfigRadioInt* m_radio_cursor_visible_always;
  ConfigBool* m_checkbox_lock_mouse;
};
