// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;
class QVBoxLayout;
class ToolTipCheckBox;
class ToolTipComboBox;
class ToolTipPushButton;

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
  void AddDescriptions();

  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(Core::State state);
  void UpdateDescriptionsUsingHardcoreStatus();

  // Widgets
  QVBoxLayout* m_main_layout;
  ToolTipComboBox* m_combobox_speedlimit;
  ToolTipComboBox* m_combobox_update_track;
  ToolTipComboBox* m_combobox_fallback_region;
  ConfigBool* m_checkbox_dualcore;
  ConfigBool* m_checkbox_cheats;
  ConfigBool* m_checkbox_override_region_settings;
  ConfigBool* m_checkbox_auto_disc_change;
#ifdef USE_DISCORD_PRESENCE
  ToolTipCheckBox* m_checkbox_discord_presence;
#endif

// Analytics related
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  void CreateAnalytics();
  void GenerateNewIdentity();

  ToolTipPushButton* m_button_generate_new_identity;
  ToolTipCheckBox* m_checkbox_enable_analytics;
#endif
};
