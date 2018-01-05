// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;

class GeneralPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void CreateBasic();
  void CreateAdvanced();

  void LoadConfig();
  void OnSaveConfig();

  // Widgets
  QVBoxLayout* m_main_layout;
  QComboBox* m_combobox_language;
  QSlider* m_slider_speedlimit;
  QCheckBox* m_checkbox_force_ntsc;
  QLabel* m_label_speedlimit;

// Analytics related
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  void CreateAnalytics();
  void GenerateNewIdentity();

  QPushButton* m_button_generate_new_identity;
  QCheckBox* m_checkbox_enable_analytics;
#endif
};
