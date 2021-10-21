// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;
class QVBoxLayout;

namespace Core
{
enum class State;
}

class GeneralPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void CreateBasic();
  void CreateAutoUpdate();
  void CreateFallbackRegion();

  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(Core::State state);

  // Widgets
  QVBoxLayout* m_main_layout;
  QComboBox* m_combobox_speedlimit;
  QComboBox* m_combobox_update_track;
  QComboBox* m_combobox_fallback_region;
  QCheckBox* m_checkbox_dualcore;
  QCheckBox* m_checkbox_cheats;
  QCheckBox* m_checkbox_override_region_settings;
  QCheckBox* m_checkbox_auto_disc_change;
#ifdef USE_DISCORD_PRESENCE
  QCheckBox* m_checkbox_discord_presence;
#endif
  QLabel* m_label_speedlimit;

// Analytics related
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  void CreateAnalytics();
  void GenerateNewIdentity();

  QPushButton* m_button_generate_new_identity;
  QCheckBox* m_checkbox_enable_analytics;
#endif
};
