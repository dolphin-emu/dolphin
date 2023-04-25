// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigChoice;
class QCheckBox;
class QPushButton;
class ToolTipCheckBox;

class FreeLookWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FreeLookWidget(QWidget* parent);

private:
  void CreateLayout();
  void ConnectWidgets();

  void OnFreeLookControllerConfigured();
  void LoadSettings();
  void SaveSettings();

  ToolTipCheckBox* m_enable_freelook;
  ConfigChoice* m_freelook_control_type;
  QPushButton* m_freelook_controller_configure_button;
  QCheckBox* m_freelook_background_input;
};
