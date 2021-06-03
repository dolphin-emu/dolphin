// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QPushButton;
class ToolTipCheckBox;

class FreeLook2DWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FreeLook2DWidget(QWidget* parent);

private:
  void CreateLayout();
  void ConnectWidgets();

  void OnFreeLookControllerConfigured();
  void OnFreeLookOptionsConfigured();
  void LoadSettings();
  void SaveSettings();

  ToolTipCheckBox* m_enable_freelook;
  QPushButton* m_freelook_controller_configure_button;
  QPushButton* m_freelook_options_configure_button;
};
