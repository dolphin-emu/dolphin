// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigStringChoice;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QVBoxLayout;
class ToolTipCheckBox;

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
  void LoadConfig();
  void OnSaveConfig();
  void OnCursorVisibleMovement();
  void OnCursorVisibleNever();
  void OnCursorVisibleAlways();
  void OnLanguageChanged();

  QVBoxLayout* m_main_layout;
  ConfigStringChoice* m_combobox_language;

  ConfigStringChoice* m_combobox_theme;
  QComboBox* m_combobox_userstyle;
  QLabel* m_label_userstyle;
  ConfigBool* m_checkbox_top_window;
  ConfigBool* m_checkbox_use_builtin_title_database;
  ToolTipCheckBox* m_checkbox_show_debugging_ui;
  ConfigBool* m_checkbox_focused_hotkeys;
  ConfigBool* m_checkbox_use_covers;
  ConfigBool* m_checkbox_disable_screensaver;

  ConfigBool* m_checkbox_confirm_on_stop;
  QCheckBox* m_checkbox_use_panic_handlers;
  QCheckBox* m_checkbox_enable_osd;
  QCheckBox* m_checkbox_show_active_title;
  QCheckBox* m_checkbox_pause_on_focus_lost;
  QRadioButton* m_radio_cursor_visible_movement;
  QRadioButton* m_radio_cursor_visible_never;
  QRadioButton* m_radio_cursor_visible_always;
  QCheckBox* m_checkbox_lock_mouse;
};
