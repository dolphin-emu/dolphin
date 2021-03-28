// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class GraphicsChoice;
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
  GraphicsChoice* m_freelook_control_type;
  QPushButton* m_freelook_controller_configure_button;
};
