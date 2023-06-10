// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QWidget;
class QLabel;
class ConfigBool;
class ConfigChoice;
class ConfigFloatSlider;
class QDialogButtonBox;

class ColorCorrectionConfigWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ColorCorrectionConfigWindow(QWidget* parent);

private:
  void Create();
  void ConnectWidgets();

  ConfigBool* m_correct_color_space;
  ConfigChoice* m_game_color_space;
  ConfigFloatSlider* m_game_gamma;
  QLabel* m_game_gamma_value;
  ConfigBool* m_correct_gamma;
  ConfigBool* m_sdr_display_gamma_srgb;
  ConfigFloatSlider* m_sdr_display_custom_gamma;
  QLabel* m_sdr_display_custom_gamma_value;
  ConfigFloatSlider* m_hdr_paper_white_nits;
  QLabel* m_hdr_paper_white_nits_value;
  QDialogButtonBox* m_button_box;
};
