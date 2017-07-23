// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QVBoxLayout;

class InterfacePane final : public QWidget
{
  Q_OBJECT
public:
  explicit InterfacePane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void CreateUI();
  void CreateInGame();
  void ConnectLayout();
  void LoadConfig();
  void OnSaveConfig();

  QVBoxLayout* m_main_layout;
  QComboBox* m_combobox_language;

  QComboBox* m_combobox_theme;
  QCheckBox* m_checkbox_auto_window;
  QCheckBox* m_checkbox_top_window;
  QCheckBox* m_checkbox_render_to_window;
  QCheckBox* m_checkbox_use_builtin_title_database;

  QCheckBox* m_checkbox_confirm_on_stop;
  QCheckBox* m_checkbox_use_panic_handlers;
  QCheckBox* m_checkbox_enable_osd;
  QCheckBox* m_checkbox_show_active_title;
  QCheckBox* m_checkbox_pause_on_focus_lost;
  QCheckBox* m_checkbox_hide_mouse;
};
